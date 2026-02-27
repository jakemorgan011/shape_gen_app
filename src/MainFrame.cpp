#include "MainFrame.h"
#include "ShapeTrainer.h"
#include "ExploreFrame.h"
#include <wx/menu.h>
#include <wx/button.h>
#include <wx/sizer.h>

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(wxID_EXIT,  MainFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MainFrame::OnAbout)
    EVT_BUTTON(ID_TRAIN_BUTTON, MainFrame::OnTrain)
    EVT_COMMAND(wxID_ANY, EVT_TRAINING_COMPLETE, MainFrame::OnTrainingComplete)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxPoint(100, 100), wxSize(1024, 768)) {

    wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_EXIT, "E&xit\tAlt-X", "Quit this program");

    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT, "&About\tF1", "Show about dialog");

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);

    m_trainer = std::make_shared<ShapeTrainer>(this);  // numVerts inferred from anchor data at training time

    wxGLAttributes vAttrs;
    vAttrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(24).EndList();

    m_glCanvas   = new GLCanvas(this, vAttrs, m_trainer.get());
    m_trainButton = new wxButton(this, ID_TRAIN_BUTTON, "Train");

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_glCanvas,    1, wxEXPAND);
    sizer->Add(m_trainButton, 0, wxEXPAND | wxALL, 4);
    SetSizer(sizer);

    m_audioEngine = std::make_unique<AudioEngine>();
    m_audioEngine->initialize();
    m_audioEngine->start();

    m_exploreFrame = new ExploreFrame(this, m_trainer.get());

    // Position ExploreFrame to the right of MainFrame so they don't overlap
    wxRect mainRect = GetRect();
    m_exploreFrame->SetPosition(wxPoint(mainRect.GetRight() + 10, mainRect.GetTop()));

    m_exploreFrame->Show(true);
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

void MainFrame::OnTrain(wxCommandEvent& /*event*/) {
    if (!m_trainer || m_trainer->isTraining()) return;

    std::vector<AnchorData> anchors = m_glCanvas->CollectAnchorData();
    if (anchors.size() < 2) return;  // need at least 2 in-map anchors to train

    m_trainButton->Disable();
    m_trainButton->SetLabel("Training...");

    m_trainer->startTraining(std::move(anchors));
}

void MainFrame::OnTrainingComplete(wxCommandEvent& /*event*/) {
    m_trainButton->Enable();
    m_trainButton->SetLabel("Train");

    if (m_exploreFrame)
        m_exploreFrame->notifyModelUpdated();
}
