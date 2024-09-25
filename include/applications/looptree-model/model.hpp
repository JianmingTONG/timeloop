#include <boost/serialization/vector.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>

#include "mapping/parser.hpp"
#include "mapping/arch-properties.hpp"
#include "mapping/constraints.hpp"
#include "compound-config/compound-config.hpp"
#include "model/sparse-optimization-parser.hpp"
#include "mapping/fused-mapping.hpp"
#include "loop-analysis/isl-ir.hpp"

//--------------------------------------------//
//                Application                 //
//--------------------------------------------//

namespace application
{

class LooptreeModel
{
 public:
  struct Result
  {
    std::map<
      problem::EinsumId,
      std::tuple<std::vector<analysis::SpaceTime>, std::string>
    > ops;

    std::map<
      std::tuple<mapping::BufferId, problem::DataSpaceId, mapping::NodeID>,
      std::tuple<std::vector<analysis::SpaceTime>, std::string>
    > fills;

    std::map<
      std::tuple<mapping::BufferId, problem::DataSpaceId, mapping::NodeID>,
      std::tuple<std::vector<analysis::SpaceTime>, std::string>
    > fills_by_parent;

    std::map<
      std::tuple<mapping::BufferId, problem::DataSpaceId, mapping::NodeID>,
      std::tuple<std::vector<analysis::SpaceTime>, std::string>
    > fills_by_peer;

    std::map<
      std::tuple<mapping::BufferId, problem::DataSpaceId, mapping::NodeID>,
      std::tuple<std::vector<analysis::SpaceTime>, std::string>
    > occupancy;

    std::map<
      problem::EinsumId,
      std::tuple<std::vector<analysis::SpaceTime>, std::string>
    > temporal_steps;
  };

 protected:
  // Critical state.
  problem::FusedWorkload workload_;
  mapping::FusedMapping mapping_;

 public:

  LooptreeModel(config::CompoundConfig* config);
  LooptreeModel(const problem::FusedWorkload& workload,
                const mapping::FusedMapping& mapping);

  // This class does not support being copied
  LooptreeModel(const LooptreeModel&) = delete;
  LooptreeModel& operator=(const LooptreeModel&) = delete;

  ~LooptreeModel();

  // Run the evaluation.
  Result Run();
};

} // namespace application