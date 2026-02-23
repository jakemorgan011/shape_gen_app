#pragma once

#include <wx/wx.h>
#include "GLCanvas.h"
#include "AudioEngine.h"
#include <memory>

class ShapeTrainer;
class ExploreFrame;

enum {
    ID_TRAIN_BUTTON = wxID_HIGHEST + 1
};

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title);
    virtual ~MainFrame();

private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnTrain(wxCommandEvent& event);
    void OnTrainingComplete(wxCommandEvent& event);

    GLCanvas*    m_glCanvas  = nullptr;
    std::unique_ptr<AudioEngine> m_audioEngine;

    std::shared_ptr<ShapeTrainer> m_trainer;
    ExploreFrame* m_exploreFrame = nullptr;
    wxButton*     m_trainButton  = nullptr;

    wxDECLARE_EVENT_TABLE();
};
