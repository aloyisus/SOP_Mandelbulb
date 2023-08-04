#ifndef __SOP_Mandelbulb_h__
#define __SOP_Mandelbulb_h__

#include <SOP/SOP_Node.h>


namespace HDK_Sample {

    class SOP_Mandelbulb : public SOP_Node
    {
    public:
        static OP_Node* myConstructor(OP_Network* net, const char* name, OP_Operator* op);
        static PRM_Template myTemplateList[];

    protected:
        SOP_Mandelbulb(OP_Network* net, const char* name, OP_Operator* op);

        ~SOP_Mandelbulb() override {}

        OP_ERROR cookMySop(OP_Context& context) override;

    };

} // End the HDK_Sample namespace

#endif

