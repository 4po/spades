#pragma once
#include "barcode_mapper.hpp"
#include "tslr_extension_chooser.hpp"

namespace omnigraph {

    template<class Graph, typename distance_t = size_t>
    class LengthPutChecker : public VertexPutChecker<Graph, distance_t> {
        typedef typename Graph::VertexId VertexId;
        typedef typename Graph::EdgeId EdgeId;
        const Graph& g_;
        const distance_t bound_;
        vector <EdgeId>& candidates_;
    public:
        LengthPutChecker(distance_t bound, const Graph& g, vector <EdgeId>& candidates) : VertexPutChecker<Graph, distance_t>(),
                                                             g_(g), bound_(bound), candidates_(candidates) { }
        bool Check(VertexId, EdgeId edge, distance_t) const {

            if (g_.length(edge) < bound_) {
                return true;
            } else {
                candidates_.push_back(edge);
                return false;
            }
        }
    };

    template <class Graph>
    class LengthDijkstra {
        using LengthBoundedDijkstraSettings = ComposedDijkstraSettings<Graph,
                LengthCalculator<Graph>,
                BoundProcessChecker<Graph>,
                LengthPutChecker<Graph>,
                ForwardNeighbourIteratorFactory<Graph> >;

        using LengthBoundedDijkstra = Dijkstra<Graph, LengthBoundedDijkstraSettings>;

    public:
        static LengthBoundedDijkstra CreateLengthBoundedDijkstra(const Graph &graph, size_t distance_bound,
                                    size_t length_bound, vector <EdgeId>& candidates, size_t max_vertex_number = -1ul){
            return LengthBoundedDijkstra(graph, LengthBoundedDijkstraSettings(
                    LengthCalculator<Graph>(graph),
                    BoundProcessChecker<Graph>(distance_bound),
                    LengthPutChecker<Graph>(length_bound, graph, candidates),
                    ForwardNeighbourIteratorFactory<Graph>(graph)),
                                         max_vertex_number);
        }
    };

    template<class Graph, typename distance_t = size_t>
    class BarcodePutChecker : public VertexPutChecker<Graph, distance_t> {
        typedef typename Graph::VertexId VertexId;
        typedef typename Graph::EdgeId EdgeId;
        typedef shared_ptr<tslr_resolver::BarcodeMapper> Bmapper;
        const Graph& g_;
        const distance_t length_bound_;
        const double barcode_threshold_;
        const distance_t barcode_len_;
        Bmapper mapper_;
        EdgeId decisive_edge_;;
        const ScaffoldingUniqueEdgeStorage& unique_storage_;
        vector <EdgeId>& candidates_;
    private:
        bool CheckNumberOfBarcodes(size_t first_barcodes, size_t second_barcodes, double threshold) const {
            return (first_barcodes > threshold * (double)second_barcodes and
                    second_barcodes > threshold * (double)first_barcodes);
        }

    public:
        BarcodePutChecker(const Graph& g, 
            const distance_t& length_bound, 
            const double& barcode_threshold,
            const distance_t& barcode_len,
            const Bmapper& mapper,
            const EdgeId& decisive_edge,
            const ScaffoldingUniqueEdgeStorage& unique_storage,
            vector<EdgeId>& candidates) : VertexPutChecker<Graph, distance_t> (),
                                                             g_(g), 
                                                             length_bound_(length_bound),
                                                             barcode_threshold_(barcode_threshold),
                                                             barcode_len_(barcode_len),
                                                             mapper_(mapper), 
                                                             decisive_edge_(decisive_edge),
                                                             unique_storage_(unique_storage),
                                                             candidates_(candidates) { }

        //This method performs simple checks on nearby edges to compose candidate set for further filtering
        bool Check(VertexId, EdgeId edge, distance_t dist) const {
            DEBUG("Checking edge " << edge.int_id());
            DEBUG("Length " << g_.length(edge)) 
            DEBUG("Decisive edge " << decisive_edge_.int_id())
            DEBUG("Intersection " << mapper_->GetIntersectionSize(decisive_edge_, edge))
            DEBUG("Normalized intersection (first) "
                          << mapper_->GetIntersectionSizeNormalizedByFirst(decisive_edge_, edge))
            DEBUG("Normalized intersection (second) "
                          << mapper_->GetIntersectionSizeNormalizedBySecond(decisive_edge_, edge))
            size_t gap = dist - g_.length(edge);
            DEBUG("Gap " << gap)
            //fixme move somewhere
            const size_t barcode_len = 10000;
            DEBUG("Threshold: " << tslr_resolver::ReadCloudExtensionChooser::GetGapCoefficient(static_cast<int> (gap),
                                                                                                 barcode_len)
                                   * barcode_threshold_)

            size_t decisive_barcodes = mapper_->GetTailBarcodeNumber(decisive_edge_);
            size_t current_barcodes = mapper_->GetHeadBarcodeNumber(edge);

            DEBUG("Barcodes " << current_barcodes);
            DEBUG("Decisive edge barcodes " << decisive_barcodes);
            DEBUG("Is unique " << unique_storage_.IsUnique(edge));

            if (g_.length(edge) < length_bound_) {
                DEBUG("Short edge, passed" << endl)  //todo use short edges to reduce number of candidates
                return true;
            }
            if (!unique_storage_.IsUnique(edge)) {
                DEBUG("Long non-unique nearby edge, passed" << endl)
                return true;
            } else {
                //Barcode number should be similar on nearby genome fragments.
                if (!CheckNumberOfBarcodes(decisive_barcodes, current_barcodes,
                                           cfg::get().ts_res.barcode_number_threshold)) {
                    DEBUG("Barcode numbers significantly differ, don't put to candidates list");
                    return false;
                }
                candidates_.push_back(edge);
                DEBUG("Long unique nearby edge, put to candidates list and stop" << endl)
                return false;
            }
        }

        DECL_LOGGER("BarcodePutChecker")
    };

    template <class Graph>
    class BarcodeDijkstra {
        using BarcodeBoundedDijkstraSettings = ComposedDijkstraSettings<Graph,
                LengthCalculator<Graph>,
                BoundProcessChecker<Graph>,
                BarcodePutChecker<Graph>,
                ForwardNeighbourIteratorFactory<Graph> >;

        using BarcodeBoundedDijkstra = Dijkstra<Graph, BarcodeBoundedDijkstraSettings>;

    public:
        static BarcodeBoundedDijkstra CreateBarcodeBoundedDijkstra(const Graph &graph, 
            size_t distance_bound, 
            BarcodePutChecker<Graph>& put_checker, 
            size_t max_vertex_number = -1ul){
            return BarcodeBoundedDijkstra(graph, BarcodeBoundedDijkstraSettings(
                    LengthCalculator<Graph>(graph),
                    BoundProcessChecker<Graph>(distance_bound),
                    put_checker,
                    ForwardNeighbourIteratorFactory<Graph>(graph)),
                                         max_vertex_number);
        }
    }; 
}