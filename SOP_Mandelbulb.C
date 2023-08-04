#include "SOP_Mandelbulb.h"

#include <GA/GA_Handle.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVolume.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_ChoiceList.h>
#include <PRM/PRM_Include.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_ThreadedAlgorithm.h>
#include <UT/UT_VoxelArray.h>
#include <UT/UT_Vector3.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_Axis.h>
#include <SYS/SYS_Math.h>

#include <chrono>
#include <iostream>
#include <limits.h>
#include "math.h"

// Just comment this out to turn off the debug output
#define __DEBUG__ 1

using namespace HDK_Sample;


void
newSopOperator(OP_OperatorTable* table)
{
    table->addOperator(new OP_Operator(
        "mandelbulb",   // Internal name
        "Mandelbulb",                     // UI name
        SOP_Mandelbulb::myConstructor,    // How to build the SOP
        SOP_Mandelbulb::myTemplateList,             // My parameters
        0,                          // Min # of sources
        0,                          // Max # of sources
        nullptr,                    // Custom local variables (none)
        OP_FLAG_GENERATOR));        // Flag it as generator
}

OP_Node* SOP_Mandelbulb::myConstructor(OP_Network* net, const char* name, OP_Operator* op)
{
    return new SOP_Mandelbulb(net, name, op);
}

SOP_Mandelbulb::SOP_Mandelbulb(OP_Network* net, const char* name, OP_Operator* op)
    : SOP_Node(net, name, op)
{

}

static PRM_Name names[] = {
    PRM_Name("div",     "Divisions"),
    PRM_Name("threaded", "Use Multi-threaded"),
    PRM_Name("order", "Order"),
    PRM_Name("iterations", "Max Iterations")
};

static PRM_Default orderDefault(6);
static PRM_Default iterationsDefault(16);
static PRM_Default  divDefaults[] = {
                       PRM_Default(64),
                       PRM_Default(64),
                       PRM_Default(64),
};

// Here's a list of parameters for my node
PRM_Template
SOP_Mandelbulb::myTemplateList[] = {
    PRM_Template(PRM_FLT_J,     3, &names[0], divDefaults),
    PRM_Template(PRM_TOGGLE,     1, &names[1], PRMoneDefaults),
    PRM_Template(PRM_INT,     1, &names[2], &orderDefault),
    PRM_Template(PRM_INT,     1, &names[3], &iterationsDefault),

    PRM_Template(PRM_FLT_J,     3, &PRMoffsetName, PRMzeroDefaults),
    PRM_Template(PRM_FLT_J,     3, &PRMscaleName, PRMoneDefaults),
    PRM_Template(PRM_FLT_J,     3, &PRMrotName, PRMzeroDefaults),

    PRM_Template()      // Terminator - put at the end of the list
};

// the function that decides whether a voxel is part of the Mandelbulb or not
// see here for more information https://www.skytopia.com/project/fractal/2mandelbulb.html#formula
int mandel(float x0, float y0, float z0, int imax, int n=6) {
    float x, y, z, xnew, ynew, znew, r, theta, phi;
    int i;

    x = x0;
    y = y0;
    z = z0;

    for (i = 0; i < imax; i++) {

        r = sqrt(x * x + y * y + z * z);
        theta = atan2(sqrt(x * x + y * y), z);
        phi = atan2(y, x);

        xnew = pow(r, n) * sin(theta * n) * cos(phi * n) + x0;
        ynew = pow(r, n) * sin(theta * n) * sin(phi * n) + y0;
        znew = pow(r, n) * cos(theta * n) + z0;

        if (xnew * xnew + ynew * ynew + znew * znew > 8)
            return i;

        x = xnew;
        y = ynew;
        z = znew;
    }
    return imax;
}

OP_ERROR SOP_Mandelbulb::cookMySop(OP_Context& context)
{
    // Since we don't have inputs, we don't need to lock them.

    gdp->clearAndDestroy();

    float   now = context.getTime();

    GU_PrimVolume* vol;
    vol = (GU_PrimVolume*)GU_PrimVolume::build((GU_Detail*)gdp);

    // Set the name of the volume to density
    UT_String   name("density");
    GA_RWHandleS attrib(gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "name", 1));
    attrib.set(vol->getMapOffset(), name);

    UT_VoxelArrayWriteHandleF   handle = vol->getVoxelWriteHandle();
    UT_VoxelArrayF *vox = &*handle;

    int rx = evalInt("div", 0, now);
    int ry = evalInt("div", 1, now);
    int rz = evalInt("div", 2, now);
    int n = evalInt("order", 0, now);
    int maxiter = evalInt("iterations", 0, now);

    // Resize the array.
    handle->size(rx, ry, rz);

    UT_Vector3 offset(evalFloat("offset", 0, now), evalFloat("offset", 1, now), evalFloat("offset", 2, now));
    UT_Vector3 scale(evalFloat("s", 0, now), evalFloat("s", 1, now), evalFloat("s", 2, now));
    UT_Vector3 rotation(evalFloat("r", 0, now), evalFloat("r", 1, now), evalFloat("r", 2, now));
    rotation.degToRad();

    // Create a transformation matrix
    UT_Matrix4F mat;
    mat.identity();

    // Apply the transformations
    mat.translate(offset);
    mat.scale(scale);
    mat.rotate(UT_Axis3::XAXIS, rotation[0]); //rotation around X axis
    mat.rotate(UT_Axis3::YAXIS, rotation[1]); //rotation around Y axis
    mat.rotate(UT_Axis3::ZAXIS, rotation[2]); //rotation around Z axis

    // Here's where we actually write to the volume, applying the transform
    // matrix to the positions before they get plugged into the mandel function
    float v;
    if (!evalInt("threaded", 0, now)) {
        for (int z = 0; z < rz; z++)
        {
            for (int y = 0; y < ry; y++)
            {
                for (int x = 0; x < rx; x++)
                {
                    UT_Vector3 pos;
                    // this converts an index to voxel space 0..1
                    handle->findexToPos(UT_Vector3(x, y, z), pos);
                    // this converts from 0..1 voxel space to world space
                    pos = vol->fromVoxelSpace(pos);
                    pos *= mat;
                    if (mandel(pos[0], pos[1], pos[2], maxiter, n) < maxiter) {
                        v = 0.0;
                    }
                    else {
                        v = 1.0;
                    }
                    handle->setValue(x, y, z, v);
                }
            }
        }
    }
    else {
        UTparallelForEachNumber((exint)vox->numTiles(),
            [&](const UT_BlockedRange<exint>& r) {
                exint				curtile = r.begin();
                UT_VoxelTileIteratorF		vit;
                vit.setLinearTile(curtile, vox);
                for (vit.rewind(); !vit.atEnd(); vit.advance()) {
                    UT_Vector3 pos;
                    // this converts an index to voxel space 0..1
                    handle->findexToPos(UT_Vector3(vit.x(), vit.y(), vit.z()), pos);
                    // this converts from 0..1 voxel space to world space
                    pos = vol->fromVoxelSpace(pos);
                    pos *= mat;
                    if (mandel(pos[0], pos[1], pos[2], maxiter, n) < maxiter) {
                        v = 0.0;
                    }
                    else {
                        v = 1.0;
                    }
                    handle->setValue(vit.x(), vit.y(), vit.z(), v);
                }
            });
    }

#ifdef __DEBUG__
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
    std::cout << "Time taken to compute Mandelbulb: " << duration.count() << " microseconds" << std::endl;
#endif

    return error();
}
