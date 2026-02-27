#pragma once
// wx/translation.h defines N_(s) as a gettext helper macro which clashes with
// libtorch's ATen function_schema.h member named N_. Undef it before torch.
#ifdef N_
#undef N_
#endif
#include <torch/torch.h>
#include <wx/wx.h>
#include <glm/glm.hpp>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include "AnchorData.h"

wxDECLARE_EVENT(EVT_TRAINING_COMPLETE, wxCommandEvent);

struct ShapeNetImpl : torch::nn::Module {
    torch::nn::Sequential net{nullptr};
    ShapeNetImpl(int nv, int hd = 256) {
        net = register_module("net", torch::nn::Sequential(
            torch::nn::Linear(2,  hd), torch::nn::ReLU(),
            torch::nn::Linear(hd, hd), torch::nn::ReLU(),
            torch::nn::Linear(hd, hd), torch::nn::ReLU(),
            torch::nn::Linear(hd, nv * 3)
        ));
    }
    torch::Tensor forward(torch::Tensor xy) {
        return net->forward(xy).reshape({-1, 3});
    }
};
TORCH_MODULE(ShapeNet);

class ShapeTrainer {
public:
    ShapeTrainer(wxEvtHandler* completionTarget, int numVertices = 0);
    ~ShapeTrainer();

    bool startTraining(std::vector<AnchorData> anchors);
    bool isTraining()   const { return m_isTraining.load(); }
    bool isModelReady() const { return m_modelReady.load(); }
    std::vector<float> infer(float lx, float ly);  // thread-safe

    const std::vector<unsigned int>& getMeshIndices() const { return m_meshIndices; }

private:
    void trainLoop(std::vector<AnchorData> anchors);
    torch::Tensor smoothnessLoss();

    wxEvtHandler*              m_completionTarget;
    int                        m_numVertices;
    ShapeNet                   m_model{nullptr};
    std::mutex                 m_modelMutex;
    std::atomic<bool>          m_isTraining{false};
    std::atomic<bool>          m_modelReady{false};
    std::thread                m_trainThread;
    std::mt19937               m_rng;
    std::vector<unsigned int>  m_meshIndices;
};
