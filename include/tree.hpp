#pragma once

#include "node.hpp"
#include "random.hpp"
#include "settings.hpp"
#include <vector>
#include <data.hpp>

namespace ISUE {
  namespace RelocForests {
    class Tree {
    public:
      Tree()
      {
        root = new Node();
      };

      ~Tree()
      {
        delete root;
      };


      enum OUT { LEFT, RIGHT, TRASH };

      /*
        Evaluates weak learner. Decides whether the point should go left or right.
        Returns OUT enum value.
       */
      OUT eval_learner(Data *data, LabeledPixel pixel, DepthAdaptiveRGB *feature) 
      {
        auto response = feature->GetResponse(data, pixel);

        if (!response.second) // no depth or out of bounds
          return OUT::TRASH;
        return (OUT)(response.first >= feature->GetThreshold());
      }

      double variance(std::vector<LabeledPixel> labeled_data)
      {
        if (labeled_data.size() == 0)
          return 0.0;
        double V = (1.0f / (double)labeled_data.size());
        double sum = 0.0f;

        // calculate mean of S
        cv::Point3f tmp;
        for (auto p : labeled_data)
          tmp += p.label_;
        int size = labeled_data.size();
        cv::Point3f mean(tmp.x / size, tmp.y / size, tmp.z / size);
        
        for (auto p : labeled_data) {
          cv::Point3f val = (p.label_ - mean);
          sum += val.x * val.x  + val.y * val.y + val.z * val.z;
        }

        return V * sum;
      }


      double objective_function(std::vector<LabeledPixel> data, std::vector<LabeledPixel> left, std::vector<LabeledPixel> right)
      {
        double var = variance(data);
        double sum;

        // left
        double left_var = variance(left);
        double left_val = ((double)left.size() / (double)data.size()) * left_var;
        // right
        double right_var = variance(right);
        double right_val = ((double)right.size() / (double)data.size()) * right_var;

        sum = left_val + right_val;
        return var - sum;
      }

      /*
        Returns height from current node to root node.
      */
      int traverse_to_root(Node *node) {
        if (node == nullptr)
          return 0;
        return 1 + traverse_to_root(node->parent_);
      }

      Data *data;
      Random *random;
      Settings *settings;

      void train_recurse(Node *node, std::vector<LabeledPixel> S) 
      {
        int height = traverse_to_root(node);
        if (S.size() == 1 || height >= settings->max_tree_depth_) {
          node->is_leaf_ = true;
          node->distribution_ = S;
          return;
        }
        else if (S.size() == 0) {
          delete node;
          node = nullptr;
          return;
        }

        node->is_split_ = true;
        node->is_leaf_ = false;

        int num_candidates = 5;
        std::vector<DepthAdaptiveRGB*> candidate_params;
        std::vector<LabeledPixel> left_final, right_final;
        double minimum_objective = DBL_MAX;
        int feature = 0;


        for (int i = 0; i < num_candidates; ++i) {

          // add candidate
          candidate_params.push_back(DepthAdaptiveRGB::CreateRandom(random, settings->image_width_, settings->image_height_));

          // partition data with candidate
          std::vector<LabeledPixel> left_data, right_data;

          for (int j = 0; j < S.size(); ++j) {
            // todo throw away undefined vals

            OUT val = eval_learner(data, S.at(j), candidate_params.at(i));
            
            switch (val) {
            case LEFT:
              left_data.push_back(S.at(j));
              break;
            case RIGHT:
              right_data.push_back(S.at(j));
              break;
            case TRASH:
              // do nothing
              break;
            }

          }

          // eval tree training objective function and take best
          // todo: ensure objective function is correct
          double objective = objective_function(S, left_data, right_data);

          if (objective < minimum_objective) {
            feature = i;
            minimum_objective= objective;
            left_final = left_data;
            right_final = right_data;
          }
        }

        // set feature
        node->feature_ = candidate_params.at(feature);
        node->left_ = new Node();
        node->right_ = new Node();
        node->left_->parent_ = node->right_->parent_ = node;

        train_recurse(node->left_, left_final);
        train_recurse(node->right_, right_final);
      }

      void Train(Data *data, std::vector<LabeledPixel> labeled_data, Random *random, Settings *settings) 
      {
        this->data = data;
        this->random = random;
        this->settings = settings;
        train_recurse(this->root, labeled_data);
        // todo: mode fitting
      }

    private:
      Node *root;
    };
  }
}