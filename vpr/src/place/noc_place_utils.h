#ifndef NOC_PLACE_UTILS_H
#define NOC_PLACE_UTILS_H

#include "globals.h"
#include "noc_routing.h"
#include "place_util.h"
#include "vtr_assert.h"
#include "move_transactions.h"
#include "vtr_log.h"
#include "noc_routing_algorithm_creator.h"

// represent the maximum values of the NoC cost normalization factors //
// we need to handle the case where the agggregate bandwidth is 0, so we set this to some arbritary positive number that is greater than 1.e-9, since that is the range we expect the normalization factor to be (in Gbps)
constexpr double MAX_INV_NOC_AGGREGATE_BANDWIDTH_COST = 1.;
// we expect the latency costs to be in the picosecond range, and we don't expect it to go lower than that. So if the latency costs go below the picosecond range we trim the normalization value to below 1 picosecond
// This should be updated if the delays become lower
constexpr double MAX_INV_NOC_LATENCY_COST = 1.e12;

/* Defines how the links found in a traffic flow are updated in terms
 * of their bandwidth usage.
 */
enum class link_usage_update_state {
    /* State 1: The link usages have to be incremented as the traffic
     * flow route route has been updated 
     */
    increment,
    /* State 2: The link usages have to be decremented as the traffic flow
     * route is being removed
     */
    decrement
};

/**
 * @brief Routes all the traffic flows within the NoC and updates the link usage
 * for all links. This should be called after initial placement, where all the 
 * logical NoC router blocks have been placed for the first time and no traffic
 * flows have been routed yet. This function should also only be used once as
 * its intended use is to initialize the routes for all the traffic flows.
 * 
 */
void initial_noc_placement(void);

/**
 * @brief Goes through all the cluster blocks that were moved
 * in a single swap iteration during placement and checks to see
 * if any moved blocks were NoC routers. 
 * 
 * For each moved block that is a NoC router, all the traffic flows
 * that the router is a part of are re-routed. The noc placement
 * costs (latency and aggregate bandwdith) are also updated to
 * reflect the re-routed traffic flows.
 * 
 * If none of the moved blocks are NoC routers, then this function
 * does nothing.
 * 
 * This function should be used if the user enabled NoC optimization
 * during placement and only if the move was accepted by the placer.
 * 
 * @param blocks_affected Contains all the blocks that were moved in
 * the current placement iteration. THis includes the cluster ids of
 * the moved blocks, their previous locations and their new locations
 * after being moved.
 */
int find_affected_noc_routers_and_update_noc_costs(const t_pl_blocks_to_be_moved& blocks_affected, double& noc_aggregate_bandwidth_delta_c, double& noc_latency_delta_c, const t_noc_opts& noc_opts);

void commit_noc_costs(int number_of_affected_traffic_flows);

/**
 * @brief Routes a given traffic flow within the NoC based on where the
 * logical cluster blocks in the traffic flow are currently placed. The
 * found route is stored and returned externally.
 * 
 * First, the hard routers blocksthat represent the placed location of
 * the router cluster blocks are identified. Then the traffic flow
 * is routed and updated.
 * 
 * @param traffic_flow_id Represents the traffic flow that needs to be routed
 * @param noc_model Contains all the links and rotuters within the NoC. Used
 * to route traffic flows within the NoC.
 * @param noc_traffic_flows_storage Contains all the traffic flow information
 * within the NoC. Used to get the current traffic flow information.
 * @param noc_flows_router The packet routing algorithm used to route traffic
 * flows within the NoC.
 * @param placed_cluster_block_locations A datastructure that identifies the
 * placed grid locations of all cluster blocks.
 * @return std::vector<NocLinkId>& The found route for the traffic flow.
 */
std::vector<NocLinkId>& get_traffic_flow_route(NocTrafficFlowId traffic_flow_id, const NocStorage& noc_model, NocTrafficFlows& noc_traffic_flows_storage, NocRouting* noc_flows_router, const vtr::vector_map<ClusterBlockId, t_block_loc>& placed_cluster_block_locations);

/**
 * @brief Updates the bandwidth usages of links found in a routed traffic flow.
 * The link bandwidth usages are either incremeneted or decremented by the 
 * bandwidth of the traffic flow. If the traffic flow route is being deleted,
 * then the link bandwidth needs to be decremented. If the traffic flow
 * route has just been added then the link badnwidth needs to be incremented.
 * This function needs to be called everytime a traffic flow has been newly
 * routed.
 * 
 * @param traffic_flow_route The routed path for a traffic flow. This
 * contains a collection of links in the NoC.
 * @param noc_model Contains all the links and rotuters within the NoC. Used
 * to update link information.
 * @param how_to_update_links Determines how the bandwidths of links found
 * in the traffic flow route are updated.
 * @param traffic_flow_bandwidth The bandwidth of a traffic flow. This will
 * be used to update bandwidth usage of the links.
 */
void update_traffic_flow_link_usage(const std::vector<NocLinkId>& traffic_flow_route, NocStorage& noc_model, link_usage_update_state how_to_update_links, double traffic_flow_bandwidth);

/**
 * @brief Goes through all the traffic flows associated to a moved
 * logical router cluster block (a traffic flow is associated to a router if
 * the router is either a source or sink router of the traffic flow) and
 * re-routes them. The new routes are stored and the NoC cost is updated
 * to reflect the moved logical router cluster block.
 * 
 * @param moved_router_block_id The logical router cluster block that was moved
 * to a new location during placement.
 * @param noc_traffic_flows_storage Contains all the traffic flow information
 * within the NoC. Used to get the traffic flows associated to logical router
 * blocks.
 * @param noc_model Contains all the links and rotuters within the NoC. Used
 * to route traffic flows within the NoC.  
 * @param noc_flows_router The packet routing algorithm used to route traffic
 * flows within the NoC.
 * @param placed_cluster_block_locations A datastructure that identifies the
 * placed grid locations of all cluster blocks.
 * @param updated_traffic_flows Keeps track of traffic flows that have been
 * re-routed. Used to prevent re-rouitng the same traffic flow multiple times.
 */
void re_route_associated_traffic_flows(ClusterBlockId moved_router_block_id, NocTrafficFlows& noc_traffic_flows_storage, NocStorage& noc_model, NocRouting* noc_flows_router, const vtr::vector_map<ClusterBlockId, t_block_loc>& placed_cluster_block_locations, std::unordered_set<NocTrafficFlowId>& updated_traffic_flows, int& number_of_affected_traffic_flows);

/**
 * @brief Used to re-route all the traffic flows associated to logical
 * router blocks that were supposed to be moved during placement but are
 * back to their original positions.
 * 
 * @param blocks_affected Contains all the blocks that were moved in
 * the current placement iteration. THis includes the cluster ids of
 * the moved blocks, their previous locations and their new locations
 * after being moved.
 */
void revert_noc_traffic_flow_routes(const t_pl_blocks_to_be_moved& blocks_affected);

/**
 * @brief Removes the route of a traffic flow and updates the links to indicate
 * that the traffic flow does not use them. And then finds
 * a new route for the traffic flow and updates the links in the new route to
 * indicate that the traffic flow uses them.
 * 
 * @param traffic_flow_id The traffic flow to re-route.
 * @param noc_traffic_flows_storage Contains all the traffic flow information
 * within the NoC. Used to get the current traffic flow information.
 * @param noc_model Contains all the links and rotuters within the NoC. Used
 * to route traffic flows within the NoC.
 * @param noc_flows_router The packet routing algorithm used to route traffic
 * flows within the NoC.
 * @param placed_cluster_block_locations A datastructure that identifies the
 * placed grid locations of all cluster blocks.
 */
void re_route_traffic_flow(NocTrafficFlowId traffic_flow_id, NocTrafficFlows& noc_traffic_flows_storage, NocStorage& noc_model, NocRouting* noc_flows_router, const vtr::vector_map<ClusterBlockId, t_block_loc>& placed_cluster_block_locations);

/**
 * @brief 
 * 
 * @param new_noc_aggregate_bandwidth_cost 
 * @param new_noc_latency_cost 
 */
void recompute_noc_costs(double* new_noc_aggregate_bandwidth_cost, double* new_noc_latency_cost);

/**
 * @brief Updates all the cost normalization factors relevant to the NoC.
 * Also updates the placement cost depending the on the placment mode.
 * Handles exceptional cases so that the normalization factors do not
 * reach INF.
 * This is intended to be used to initialize the normalization factors of
 * the NoC and also at the outer loop iteration of placement to 
 * balance the NoC costs with other placment cost parameters.
 * 
 * @param costs Contains the normalization factors which need to be updated
 * @param placer_opts Determines the placement mode
 */
void update_noc_normalization_factors(t_placer_costs& costs, const t_placer_opts& placer_opts);

/**
 * @brief Calculates the aggregate bandwidth of each traffic flow in the NoC
 * and initializes local variables that keep track of the traffic flow 
 * aggregate bandwidths cost.
 * Then the total aggregate bandwidth cost is determines by summing up all
 * the individual traffic flow aggregate bandwidths.
 * 
 * This should be used after initial placement to determine the starting
 * aggregate bandwdith cost of the NoC.
 * 
 * @return double The aggregate bandwidth cost of the NoC.
 */
double comp_noc_aggregate_bandwidth_cost(void);

/**
 * @brief Calculates the latency cost of each traffic flow in the NoC
 * and initializes local variables that keep track of the traffic flow
 * latency costs. Then the total latency cost is determined by summing up all
 * the individual traffic flow latency costs.
 * 
 * This should be used after initial placement to determine the starting latency
 * cost of the NoC.
 * 
 * @return double The latency cost of the NoC.
 */
double comp_noc_latency_cost(const t_noc_opts& noc_opts);

/**
 * @brief 
 * 
 */
int check_noc_placement_costs(const t_placer_costs& costs, double error_tolerance, const t_noc_opts& noc_opts);

/**
 * @brief Determines the aggregate bandwidth cost of a routed traffic flow.
 * The cost is calculated as the number of links in the traffic flow multiplied
 * by the traffic flow bandwidth. This is then scaled by the priority of the
 * traffic flow.
 * 
 * @param traffic_flow_route The routed path for a traffic flow. This
 * contains a collection of links in the NoC.
 * @param traffic_flow_info  Contains the traffic flow bandwidth and
 * its priority.
 */
double calculate_traffic_flow_aggregate_bandwidth_cost(const std::vector<NocLinkId>& traffic_flow_route, const t_noc_traffic_flow& traffic_flow_info);

/**
 * @brief Determines the latency cost of a routed traffic flow.
 * The cost is calculated as the combination of the traffic flow
 * latency and its gap to the traffic flow latency constraint. Each
 * parameter above is scaled by a weighting factor that determines
 * the importance each term has on the placement cost. These weightings
 * are provided by the user. This is then scaled by the priority of the
 * traffic flow. 
 * 
 * 
 * @param traffic_flow_route The routed path for a traffic flow. This
 * contains a collection of links in the NoC.
 * @param noc_model Contains noc information such as the router and link
 * latencies.
 * @param traffic_flow_info Contains the traffi flow priority.
 * @param noc_opts Contains the user provided weightings of the traffic flow 
 * latency and its constraint parameters for the cost calculation.
 */
double calculate_traffic_flow_latency_cost(const std::vector<NocLinkId>& traffic_flow_route, const NocStorage& noc_model, const t_noc_traffic_flow& traffic_flow_info, const t_noc_opts& noc_opts);

void allocate_and_load_noc_placement_structs(void);

void free_noc_placement_structs(void);

#endif