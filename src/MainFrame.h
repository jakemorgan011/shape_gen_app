#pragma once

#include <wx/wx.h>
#include "GLCanvas.h"
#include "AudioEngine.h"
#include <memory>

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title);
    virtual ~MainFrame();

private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    GLCanvas* m_glCanvas;
    std::unique_ptr<AudioEngine> m_audioEngine;

    wxDECLARE_EVENT_TABLE();
};
