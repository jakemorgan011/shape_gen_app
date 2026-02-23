#pragma once
#include <wx/wx.h>
#include "ExploreCanvas.h"

class ShapeTrainer;

class ExploreFrame : public wxFrame {
public:
    ExploreFrame(wxWindow* parent, ShapeTrainer* trainer);
    void notifyModelUpdated();

private:
    void OnSize(wxSizeEvent& event);

    ExploreCanvas* m_canvas;

    wxDECLARE_EVENT_TABLE();
};
