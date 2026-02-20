#include <wx/wx.h>
#include "MainFrame.h"

class ShapeGenApp : public wxApp {
public:
    virtual bool OnInit() override {
        wxInitAllImageHandlers();

        MainFrame* frame = new MainFrame("shape_gen");
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(ShapeGenApp);
