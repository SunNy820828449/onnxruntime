#pragma once

#include "core/common/common.h"
#include "core/common/status.h"
#include "core/common/logging/logging.h"
#include "core/framework/execution_provider.h"
#include "core/framework/ml_value.h"
#include "core/platform/types.h"

namespace LotusIR {  // TODO: for testing only; see note below.
class Model;
}

namespace Lotus {
/**
 * Use this to configure an execution provider.
*/
struct ProviderOption {
  ProviderOption(const std::string& provider_type0, const ExecutionProviderInfo& provider_info0)
      : provider_type(provider_type0),
        provider_info(provider_info0) {
  }
  std::string provider_type;
  ExecutionProviderInfo provider_info;
};

enum class AllocationPlannerType {
  SIMPLE_SEQUENTIAL_PLANNER,
  SEQUENTIAL_PLANNER,
};

/**
* Configuration information for a session.
*/
struct SessionOptions {
  // TODO what are the mandatory requirements for session options? and what should be the default
  // values for the remaining options? Tune this constructor appropriately once we learn more
  // about the answers to these questions.
  SessionOptions(const vector<ProviderOption>& ep_options0)
      : ep_options(ep_options0) {
  }
  //int num_threads; // not used now until we re-introduce threadpools for async execution
  std::vector<ProviderOption> ep_options;
  bool enable_sequential_execution = true;  // TODO: should we default to sequential execution?

  // TODO: This has been added to facilitate testing only. It is not intended for production usage.
  // TODO: remove this option once we've tested sequential planner completely.
  AllocationPlannerType allocation_planner_type = AllocationPlannerType::SEQUENTIAL_PLANNER;

  string session_logid;                            ///< logger id to use for session output
  unsigned short session_log_verbosity_level = 0;  ///< applies to session load, initialization, etc
  // enable the memory pattern optimization.
  // The idea is if the input shapes are the same, we could trace the internal memory allocation
  // and generate a memory pattern for future request. So next time we could just do one allocation
  // with a big chunk for all the internal memory allocation.
  bool enable_mem_pattern = true;
  int max_num_graph_transformation_steps = 5;  // TODO choose a good default here?
  std::vector<std::unique_ptr<LotusIR::GraphTransformer>> list_graph_transformers;
};

/**
* Configuration information for a single Run.
*/
struct RunOptions {
  unsigned short run_log_verbosity_level = 0;  ///< applies to a particular Run() invocation
  std::string run_tag;                         ///< to identify logs generated by a particular Run() invocation
};

/**
* Pre-defined and custom metadata about the model.
*/
struct ModelMetadata {
  std::string producer_name;
  std::string graph_name;
  std::string domain;
  std::string description;
  int64_t version;
  std::unordered_map<std::string, std::string> custom_metadata_map;
};

/**
* Definition of input/outpus. Use this to get names/types/shapes.
*/
struct NodeArgDef {
  std::string name;
  std::string data_type;
  std::vector<int64_t> shape;
};

using InputDefList = std::vector<NodeArgDef>;
using OutputDefList = std::vector<NodeArgDef>;

using NameMLValMap = std::unordered_map<std::string, MLValue>;

/**
* @brief This is the main class used to Run a model.
* Sample simple usage:
*   ExecutionProviderInfo epi;
*  ProviderOption po{"CPUExecutionProvider", epi};
*  SessionOptions so(vector<ProviderOption>{po});
*  InferenceSession session_object{so};
*  Common::Status status = session_object.Load(MODEL_URI);
*  Common::Status status = session_object.Initialize();
*
*  NameMLValMap feeds;
*  feeds.insert({});
*  ...
*  std::vector<std::string> output_names;
*  output_names.insert(...);
*  ...
*  std::vector<MLValue> fetches;
*  Common::Status status = session_object.Run(run_options, feeds, output_names, &fetches);
*  process the output here...
*/
class InferenceSession {
 public:
  /**
     Create a new InferenceSession
     @param session_options Session options.
     @param logging_manager 
       Optional logging manager instance that will enable per session logger output using
       session_options.session_logid as the logger id in messages.
       If nullptr, the default LoggingManager MUST have been created previously as it will be used
       for logging. This will use the default logger id in messages.
       See core/common/logging/logging.h for details on how to do that, and how LoggingManager::DefaultLogger works.
     */
  explicit InferenceSession(const SessionOptions& session_options,
                            Logging::LoggingManager* logging_manager = nullptr);
  ~InferenceSession();

  /**
  * Load an ONNX model.
  * @param model_uri absolute path of the model file.
  * @return OK if success.
  */
  Common::Status Load(const std::string& model_uri);

  /**
  * Load an ONNX model.
  * @param istream object of the model.
  * @return OK if success.
  */
  Common::Status Load(std::istream& model_istream);

  /**
  * FOR TESTING ONLY. Load an ONNX model
  * TODO: should we expose Model in this API? This was done for now
  * to make testing easier.
  * @param model_uri absolute path of the model file.
  * @return OK if success.
  */
  Common::Status Load(std::unique_ptr<LotusIR::Model> p_model);

  /**
  * Initializes a previously loaded model. Initialization includes but is not
  * limited to graph transformations, construction of kernels, etc.
  * This method assumes that a method has been loaded previously.
  * @return OK if success
  */
  Common::Status Initialize();

  /**
  * Run a pre-loaded and pre-intialized model.
  * Multiple threads are allowed to run this function; hence its thread-safe.
  * @param feeds named inputs owned by client code and should not be changed during
  *        execution of this function.
  * @param output_names output names
  * @param p_fetches output values in the order specified by output_names.
  *        This should not be changed during execution of this function.
  * @return OK if success.
  */
  Common::Status Run(const NameMLValMap& feeds,
                     const std::vector<std::string>& output_names,
                     std::vector<MLValue>* p_fetches);

  /**
  * See Run(const NameMLValMap& feeds, const std::vector<std::string>& output_names, std::vector<MLValue>* p_fetches)
  * for details.
  * @param run_options use this to tune the Run call to your needs.
  */
  Common::Status Run(const RunOptions& run_options,
                     const NameMLValMap& feeds,
                     const std::vector<std::string>& output_names,
                     std::vector<MLValue>* p_fetches);

  /**
  * TEST ONLY: This API exists to facilitate testing only since today the ONNX model
  * input/outputs don't have names. Issue: https://github.com/onnx/onnx/issues/679.
  * Fetches all possible outputs of the model. The order of the outputs is as obtained
  * from Graph->GetOutputs().
  * See Run(const NameMLValMap& feeds, const std::vector<std::string>& output_names, std::vector<MLValue>* p_fetches)
  * for details.
  * @return OK if success.
  */
  Common::Status Run(const NameMLValMap& feeds,
                     std::vector<MLValue>* p_fetches);

  /**
  * @return pair.first = OK; FAIL otherwise. pair.second is non-NULL when pair.first = OK.
  */
  std::pair<Common::Status, const ModelMetadata*> GetModelMetadata() const;

  /**
  * Get all input definitions of the model. This does not include weights. Use this
  * to get the name/type/shapes of the inputs.
  * @return pair.first = OK; FAIL otherwise. pair.second is non-NULL when pair.first = OK.
  */
  std::pair<Common::Status, const InputDefList*> GetInputs() const;

  /**
  * Get all output definitions of the model. Use this to get the name/type/shapes of the outputs.
  * @return pair.first = OK; FAIL otherwise. pair.second is non-NULL when pair.first = OK.
  */
  std::pair<Common::Status, const OutputDefList*> GetOutputs() const;

  /**
  * Get current num threads running Run.
  */
  int GetCurrentNumRuns();

 private:
  LOTUS_DISALLOW_COPY_ASSIGN_AND_MOVE(InferenceSession);

  class Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace Lotus
