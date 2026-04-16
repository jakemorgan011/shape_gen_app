#include "ShapeTrainer.h"
#include <wx/log.h>

wxDEFINE_EVENT(EVT_TRAINING_COMPLETE, wxCommandEvent);

ShapeTrainer::ShapeTrainer(wxEvtHandler* completionTarget, int numVertices)
    : m_completionTarget(completionTarget)
    , m_numVertices(numVertices)
    , m_rng(std::random_device{}())
{
}

ShapeTrainer::~ShapeTrainer() {
    if (m_trainThread.joinable())
        m_trainThread.join();
}

bool ShapeTrainer::startTraining(std::vector<AnchorData> anchors) {
    if (m_isTraining.load()) return false;

    // Join previous thread if done
    if (m_trainThread.joinable())
        m_trainThread.join();

    m_isTraining.store(false);
    m_modelReady.store(false);

    // Store mesh topology from first anchor (shared across all anchors)
    if (!anchors.empty())
        m_meshIndices = anchors[0].indices;

    // Infer actual vertex count from anchor data (avoids hardcoded mismatch with Assimp output)
    if (!anchors.empty() && !anchors[0].vertices.empty())
        m_numVertices = static_cast<int>(anchors[0].vertices.size() / 3);

    if (m_numVertices <= 0) {
        wxLogError("ShapeTrainer: no valid anchor vertex data, aborting training.");
        return false;
    }

    m_isTraining.store(true);
    {
        std::lock_guard<std::mutex> lock(m_modelMutex);
        m_model = ShapeNet(m_numVertices);
    }

    m_trainThread = std::thread([this, anchors = std::move(anchors)]() mutable {
        trainLoop(std::move(anchors));
    });
    return true;
}

void ShapeTrainer::trainLoop(std::vector<AnchorData> anchors) {
    try {
        const int N = static_cast<int>(anchors.size());

        // Build [N,2] latent coordinate tensor
        std::vector<float> xyVec;
        xyVec.reserve(N * 2);
        for (const auto& a : anchors) {
            xyVec.push_back(a.lx);
            xyVec.push_back(a.ly);
        }
        torch::Tensor xy_targets = torch::from_blob(xyVec.data(), {N, 2}).clone();

        // Build [N, numVerts, 3] vertex target tensor
        // Each anchor's vertices array is flattened xyz of size numVerts*3
        std::vector<float> vVec;
        vVec.reserve(N * m_numVertices * 3);
        for (const auto& a : anchors) {
            int expected = m_numVertices * 3;
            int got      = static_cast<int>(a.vertices.size());
            int toCopy   = std::min(got, expected);
            for (int k = 0; k < toCopy; ++k)
                vVec.push_back(a.vertices[k]);
            for (int k = toCopy; k < expected; ++k)
                vVec.push_back(0.0f);
        }
        torch::Tensor v_targets_3d = torch::from_blob(vVec.data(),
            {N, m_numVertices, 3}).clone();

        torch::optim::Adam optimizer(m_model->parameters(),
            torch::optim::AdamOptions(3e-4));

        for (int epoch = 0; epoch < 1000; ++epoch) {
            optimizer.zero_grad();

            torch::Tensor pred = m_model->forward(xy_targets);  // [N, numVerts, 3]
            pred = pred.reshape({N, m_numVertices, 3});

            torch::Tensor loss = torch::mse_loss(pred, v_targets_3d)
                               + 0.1f * smoothnessLoss();

            loss.backward();
            optimizer.step();
        }

        {
            std::lock_guard<std::mutex> lock(m_modelMutex);
            m_modelReady.store(true);
        }
        m_isTraining.store(false);

    } catch (const c10::Error& e) {
        wxLogError("ShapeTrainer c10::Error: %s", e.what());
        m_isTraining.store(false);
    } catch (const std::exception& e) {
        wxLogError("ShapeTrainer exception: %s", e.what());
        m_isTraining.store(false);
    }

    // Always post the complete event so the UI unlocks
    wxQueueEvent(m_completionTarget, new wxCommandEvent(EVT_TRAINING_COMPLETE));
}

torch::Tensor ShapeTrainer::smoothnessLoss() {
    std::uniform_real_distribution<float> dist(0.05f, 0.95f);
    std::uniform_real_distribution<float> delta(-0.05f, 0.05f);

    std::vector<float> pts1, pts2;
    pts1.reserve(20);
    pts2.reserve(20);
    for (int k = 0; k < 10; ++k) {
        float x = dist(m_rng);
        float y = dist(m_rng);
        pts1.push_back(x);
        pts1.push_back(y);
        pts2.push_back(x + delta(m_rng));
        pts2.push_back(y + delta(m_rng));
    }

    torch::Tensor t1 = torch::from_blob(pts1.data(), {10, 2}).clone();
    torch::Tensor t2 = torch::from_blob(pts2.data(), {10, 2}).clone();

    torch::Tensor out1 = m_model->forward(t1);
    torch::Tensor out2 = m_model->forward(t2);
    return torch::mse_loss(out1, out2);
}

std::vector<float> ShapeTrainer::infer(float lx, float ly) {
    if (!m_modelReady.load()) return {};

    std::lock_guard<std::mutex> lock(m_modelMutex);
    torch::NoGradGuard no_grad;

    float xy[2] = {lx, ly};
    torch::Tensor input = torch::from_blob(xy, {1, 2}).clone();

    torch::Tensor out = m_model->forward(input);  // [numVerts, 3]

    const float* ptr = out.data_ptr<float>();
    int total = m_numVertices * 3;
    return std::vector<float>(ptr, ptr + total);
}
