//
// Created by andrey on 04.12.15.
//

#pragma once
#include "common/modules/path_extend/read_cloud_path_extend/read_cloud_connection_conditions.hpp"
#include "scaffold_graph.hpp"


namespace path_extend {
namespace scaffold_graph {


//Iterface
class ScaffoldGraphConstructor {

public:
    virtual shared_ptr<ScaffoldGraph> Construct() = 0;
};

//Basic scaffold graph constructor functions
class BaseScaffoldGraphConstructor: public ScaffoldGraphConstructor {
protected:
    shared_ptr<ScaffoldGraph> graph_;

    BaseScaffoldGraphConstructor(const debruijn_graph::Graph& assembly_graph) {
        graph_ = make_shared<ScaffoldGraph>(assembly_graph);
    }

    void ConstructFromSingleCondition(const shared_ptr<ConnectionCondition> condition,
                                      bool use_terminal_vertices_only);

    void ConstructFromConditions(vector<shared_ptr<ConnectionCondition>> &connection_conditions,
                                 bool use_terminal_vertices_only = false);

    void ConstructFromSet(const set<EdgeId> edge_set,
                          vector<shared_ptr<ConnectionCondition>> &connection_conditions,
                          bool use_terminal_vertices_only = false);

    void ConstructFromEdgeConditions(func::TypedPredicate<typename Graph::EdgeId> edge_condition,
                                     vector<shared_ptr<ConnectionCondition>> &connection_conditions,
                                     bool use_terminal_vertices_only = false);
};


class SimpleScaffoldGraphConstructor: public BaseScaffoldGraphConstructor {
protected:
    const set<EdgeId>& edge_set_;
    vector<shared_ptr<ConnectionCondition>>& connection_conditions_;

public:
    SimpleScaffoldGraphConstructor(const debruijn_graph::Graph& assembly_graph,
                                    const set<EdgeId>& edge_set,
                                    vector<shared_ptr<ConnectionCondition>> &connection_conditions):
        BaseScaffoldGraphConstructor(assembly_graph),
        edge_set_(edge_set), connection_conditions_(connection_conditions) {}

    shared_ptr<ScaffoldGraph> Construct() override;
};

class DefaultScaffoldGraphConstructor: public SimpleScaffoldGraphConstructor {
protected:
    func::TypedPredicate<typename Graph::EdgeId> edge_condition_;

public:
    DefaultScaffoldGraphConstructor(const debruijn_graph::Graph& assembly_graph,
                                    const set<EdgeId>& edge_set,
                                    vector<shared_ptr<ConnectionCondition>> &connection_conditions,
                                    func::TypedPredicate<typename Graph::EdgeId> edge_condition):
        SimpleScaffoldGraphConstructor(assembly_graph, edge_set, connection_conditions),
        edge_condition_(edge_condition)
    {}

    shared_ptr<ScaffoldGraph> Construct() override;
};

class PredicateScaffoldGraphConstructor: public BaseScaffoldGraphConstructor {
 public:
    typedef path_extend::ScaffoldEdgePredicate EdgePairPredicate;
 protected:
    const ScaffoldGraph& old_graph_;
    const shared_ptr<EdgePairPredicate> predicate_;

 public:
    PredicateScaffoldGraphConstructor(const Graph& assembly_graph,
                                      const ScaffoldGraph& old_graph_,
                                      const shared_ptr<EdgePairPredicate>& predicate_);

    shared_ptr<ScaffoldGraph> Construct() override;
 protected:
    void ConstructFromGraphAndPredicate(const ScaffoldGraph& old_graph, const shared_ptr<EdgePairPredicate> predicate);

};

class ScoreFunctionScaffoldGraphConstructor: public BaseScaffoldGraphConstructor {
    typedef path_extend::EdgePairScoreFunction EdgePairScoreFunction;
 protected:
    const ScaffoldGraph& old_graph_;
    const shared_ptr<EdgePairScoreFunction> score_function_;
    const double score_threshold_;
    const size_t num_threads_;
 public:
    ScoreFunctionScaffoldGraphConstructor(const Graph& assembly_graph,
                                          const ScaffoldGraph& old_graph_,
                                          const shared_ptr<EdgePairScoreFunction>& score_function_,
                                          const double score_threshold, size_t num_threads);

    shared_ptr<ScaffoldGraph> Construct() override;
 protected:
    void ConstructFromGraphAndScore(const ScaffoldGraph& graph, const shared_ptr<EdgePairScoreFunction> score_function,
                                    double score_threshold, size_t threads);
    DECL_LOGGER("ScoreFunctionScaffoldGraphConstructor")
};


} //scaffold_graph
} //path_extend

