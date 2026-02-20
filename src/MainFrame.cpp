#include "MainFrame.h"
#include <wx/menu.h>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(wxID_EXIT, MainFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1024, 768)) {

    wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_EXIT, "E&xit\tAlt-X", "Quit this program");

    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT, "&About\tF1", "Show about dialog");

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);

    CreateStatusBar(2);
    SetStatusText("shape_gen");

    wxGLAttributes vAttrs;
    vAttrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(24).EndList();

    m_glCanvas = new GLCanvas(this, vAttrs);

    m_audioEngine = std::make_unique<AudioEngine>();
    m_audioEngine->initialize();
    m_audioEngine->start();
}

MainFrame::~MainFrame() {
    if (m_audioEngine) {
        m_audioEngine->stop();
    }
}

void MainFrame::OnExit(wxCommandEvent& /*event*/) {
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent& /*event*/) {
    wxMessageBox("shape_gen - wxWidgets + OpenGL + Audio",
                 "About", wxOK | wxICON_INFORMATION);
}
