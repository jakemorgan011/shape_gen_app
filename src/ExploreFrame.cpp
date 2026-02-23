#include "ExploreFrame.h"
#include "ShapeTrainer.h"
#include <algorithm>

wxBEGIN_EVENT_TABLE(ExploreFrame, wxFrame)
    EVT_SIZE(ExploreFrame::OnSize)
wxEND_EVENT_TABLE()

ExploreFrame::ExploreFrame(wxWindow* parent, ShapeTrainer* trainer)
    : wxFrame(parent, wxID_ANY, "Explore", wxDefaultPosition, wxSize(512, 512),
              wxDEFAULT_FRAME_STYLE)
{
    wxGLAttributes glAttrs;
    glAttrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(24).EndList();

    m_canvas = new ExploreCanvas(this, glAttrs, trainer);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_canvas, 1, wxEXPAND);
    SetSizer(sizer);
}

void ExploreFrame::notifyModelUpdated() {
    if (m_canvas)
        m_canvas->onModelUpdated();
}

void ExploreFrame::OnSize(wxSizeEvent& event) {
    int side = std::min(event.GetSize().GetWidth(), event.GetSize().GetHeight());
    if (event.GetSize().GetWidth() != side || event.GetSize().GetHeight() != side) {
        SetClientSize(side, side);
        return;  // Don't skip — prevents recursive resize loop
    }
    event.Skip();
}
