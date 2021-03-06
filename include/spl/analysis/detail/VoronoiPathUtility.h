/*
 * VoronoiPathUtility.h
 *
 *  Created on: Feb 18, 2014
 *      Author: Martin Uhrin
 */

#ifndef VORONOI_PATH_UTILITY_DETAIL_H
#define VORONOI_PATH_UTILITY_DETAIL_H

// INCLUDES ///////////////////

#include "spl/SSLibAssert.h"

#include <map>
#include <set>

#include <boost/foreach.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/unordered_map.hpp>

#include <CGAL/Arr_observer.h>
#include <CGAL/Point_2.h>
#include <CGAL/Segment_2.h>
#include <CGAL/Cartesian_converter.h>

//#define DEBUG_VORONOI_PATH_UTILITY

// FORWARD DECLARATIONS ///////

// DEFINITION ///////////////////////

namespace spl {
namespace analysis {
namespace detail {

template< typename MapTraits, typename VD>
  struct MapBuilder;

template< typename Handle>
  struct HandleHash : std::unary_function< Handle, std::size_t>
  {
    std::size_t
    operator()(const Handle & h) const
    {
      return boost::hash_value(h.ptr());
    }
  };

template< typename MapTraits>
  class MapLabeller : CGAL::Arr_observer< typename MapTraits::Arrangement>
  {
    typedef typename MapTraits::Arrangement Map;
    typedef typename MapTraits::Bezier Bezier;
  public:
    typedef std::pair< boost::optional< typename MapTraits::Label>,
        boost::optional< typename MapTraits::Label> > EdgeLabels;

    MapLabeller(Map * const map) :
        myMap(map)
    {
      this->attach(*myMap);
    }
    virtual
    ~MapLabeller()
    {
      this->detach();
    }

    void
    setEdgeLabels(const EdgeLabels & labels)
    {
      myEdgeLabels = labels;
    }

    void
    setNextEdgeBezier(const Bezier & bezier)
    {
      myNextEdgeBezier = bezier;
    }

  private:
    virtual void
    after_create_edge(typename Map::Halfedge_handle e)
    {
      assignLabels(myEdgeLabels, e);
      if(myNextEdgeBezier)
      {
        e->data().bezier = *myNextEdgeBezier;
        if(e->data().bezier->control[0] != e->source()->point())
          e->data().bezier->reverse();
        //SSLIB_ASSERT(e->data().bezier->control[0] == e->source()->point());
        myNextEdgeBezier.reset();
      }
    }

    virtual void
    before_split_edge(typename Map::Halfedge_handle e,
        typename Map::Vertex_handle v,
        const typename Map::X_monotone_curve_2 & c1,
        const typename Map::X_monotone_curve_2 & c2)
    {
      // Save the labels from the edge about to be split
      mySplitEdgeLabels.first = e->data().label;
      mySplitEdgeLabels.second = e->twin()->data().label;
    }

    virtual void
    after_split_edge(typename Map::Halfedge_handle e1,
        typename Map::Halfedge_handle e2)
    {
      // Assign the labels from the edge before splitting to these new edges
      assignLabels(mySplitEdgeLabels, e1);
      assignLabels(mySplitEdgeLabels, e2);
      mySplitEdgeLabels.first.reset();
      mySplitEdgeLabels.second.reset();
    }

    void
    assignLabels(const EdgeLabels & labels,
        typename Map::Halfedge_handle e) const
    {
      // Check which way around we got the halfedge.  The labels will always
      // apply relative to the direction of the curve i.e. from
      // curve.source() -> curve.target():
      //
      // labels.first = halfedge
      // labels.second = halfedge.twin()

      //if(e->curve().source().is_same(e->source()->point()))
      if(e->curve().source() == e->source()->point())
      {
        e->data().label = labels.first;
        e->twin()->data().label = labels.second;
      }
      else // must have the halfedge going the other way
      {
        e->data().label = labels.second;
        e->twin()->data().label = labels.first;
      }
    }

    Map * const myMap;
    EdgeLabels myEdgeLabels;
    EdgeLabels mySplitEdgeLabels;
    boost::optional< Bezier> myNextEdgeBezier;
  };

template< typename MapTraits, typename VD>
  class EdgeDelaunayVertexMapper : CGAL::Arr_observer<
      typename MapTraits::Arrangement>
  {
    typedef typename MapTraits::Arrangement Map;
    typedef typename VD::Delaunay_graph DG;
    typedef typename DG::Vertex_handle DV;
    // Left and right Delaunay vertices
    typedef std::pair< DV, DV> DelaunayVertices;
    typedef std::pair< typename Map::Halfedge_handle, DV> EdgeVertexPair;
  public:
    typedef boost::unordered_map< typename Map::Halfedge_handle, DV,
        HandleHash< typename Map::Halfedge_handle> > EdgeVertexMap;

    EdgeDelaunayVertexMapper(Map * const map) :
        myMap(map)
    {
      this->attach(*myMap);
    }
    virtual
    ~EdgeDelaunayVertexMapper()
    {
      this->detach();
    }

    void
    setAdjacentDelaunayVertices(const DV & left, const DV & right)
    {
      myVertices = std::make_pair(left, right);
    }
    const EdgeVertexMap &
    edgeVertexMap()
    {
      return myEdgeVertexMap;
    }

    void
    populateFacePoints(const DG & delaunay)
    {
      BOOST_FOREACH(typename EdgeVertexMap::reference edgeVertexPair, myEdgeVertexMap)
      {
        const typename Map::Face_handle face = edgeVertexPair.first->face();
        if(face->data().label)
        {
          std::set< DV> discoveredVertices;
          followVertices(delaunay, face, edgeVertexPair.second,
              &discoveredVertices);
        }
      }
    }

  private:
    void
    followVertices(const DG & delaunay, const typename Map::Face_handle face,
        const DV vertex, std::set< DV> * const discovered)
    {
      //SSLIB_ASSERT(*face->data().label == vertex->info());

      face->data().points.push_back(
          MapBuilder< MapTraits, VD>::TO_EXACT(vertex->point()));
      discovered->insert(vertex);

      const typename DG::Vertex_circulator first = delaunay.incident_vertices(
          vertex);
      typename DG::Vertex_circulator cl = first;
      do
      {
        if(cl->info() == *face->data().label && discovered->count(cl) == 0)
          followVertices(delaunay, face, cl, discovered);
        ++cl;
      } while(cl != first);
    }

    virtual void
    after_create_edge(typename Map::Halfedge_handle e)
    {
      if(myVertices)
      {
        assignLabels(e, *myVertices);
        myVertices.reset();
      }
    }

    virtual void
    before_split_edge(typename Map::Halfedge_handle e,
        typename Map::Vertex_handle v,
        const typename Map::X_monotone_curve_2 & c1,
        const typename Map::X_monotone_curve_2 & c2)
    {
      static const DV EMPTY_VERTEX;

      // Try to find the halfedge being splot
      typename EdgeVertexMap::iterator it = myEdgeVertexMap.find(e);
      if(it != myEdgeVertexMap.end())
      {
        const DV leftVertex = it->second, rightVertex = myEdgeVertexMap.find(
            e->twin())->second;
        // Save the vertices from the edge about to be split
        mySplitVertices = std::make_pair(leftVertex, rightVertex);
      }
    }

    virtual void
    after_split_edge(typename Map::Halfedge_handle e1,
        typename Map::Halfedge_handle e2)
    {
      if(mySplitVertices)
      {
        // Assign the labels from the edge before splitting to these new edges
        assignLabels(e1, *mySplitVertices);
        assignLabels(e2, *mySplitVertices);
        mySplitVertices.reset();
      }
    }

    void
    assignLabels(typename Map::Halfedge_handle e,
        const DelaunayVertices & verts)
    {
      // Check which way around we got the halfedge.  The labels will always
      // apply relative to the direction of the curve i.e. from
      // curve.source() -> curve.target():

      //if(e->curve().source().is_same(e->source()->point()))
      if(e->curve().source() == e->source()->point())
      {
        myEdgeVertexMap.insert(std::make_pair(e, verts.first));
        myEdgeVertexMap.insert(std::make_pair(e->twin(), verts.second));
      }
      else // must have the halfedge going the other way
      {
        myEdgeVertexMap.insert(std::make_pair(e, verts.second));
        myEdgeVertexMap.insert(std::make_pair(e->twin(), verts.first));
      }
    }

    Map * const myMap;
    boost::optional< DelaunayVertices> myVertices;
    boost::optional< DelaunayVertices> mySplitVertices;
    EdgeVertexMap myEdgeVertexMap;
  };

template< typename MapTraits, typename VD>
  struct MapBuilder
  {
    typedef typename MapTraits::Label Label;
    typedef typename MapTraits::Arrangement Map;
    typedef VD Voronoi;
    typedef typename VD::Delaunay_graph Delaunay;
    typedef typename MapTraits::ArrPoint Point;
    typedef VoronoiPathArrangement< VD> PathArrangement;
    typedef typename PathArrangement::Path Path;
    typedef MapLabeller< MapTraits> EdgeLabeller;
    typedef typename MapTraits::Bezier MapBezier;
    typedef BezierCurve< typename Delaunay::Geom_traits> Bezier;

    typedef EdgeDelaunayVertexMapper< Map, VD> EdgeVertexMapper;

    typedef CGAL::Cartesian_converter< typename Delaunay::Geom_traits,
        typename MapTraits::Kernel> IkToEk;

    // Use this to convert the number type used by the Voronoi diagram paths
    // to the rational type used by the arrangement
    static const IkToEk TO_EXACT;

    class PathDrawer
    {
    public:
      explicit
      PathDrawer(Map * const map) :
          myMap(map)
      {
      }

      void
      moveTo(const Point & p)
      {
#ifdef DEBUG_VORONOI_PATH_UTILITY
        std::cout << "(Path.MOVETO, " << point(p) << "),\n";
#endif
        updatePos(p);
      }

      void
      lineTo(const Point & p)
      {
#ifdef DEBUG_VORONOI_PATH_UTILITY
        std::cout << "(Path.LINETO, " << point(p) << "),\n";
#endif

        if(myPos != p)
        {
          CGAL::insert(*myMap, Curve(myPos, p));
          updatePos(p);
        }
      }

      void
      curveTo(const MapBezier & bezier)
      {
#ifdef DEBUG_VORONOI_PATH_UTILITY
        for(size_t i = 1; i < 4; ++i)
        std::cout << "(Path.CURVE4, " << point(bezier.control[i]) << "),\n";
#endif
        // TODO: Check why this condition sometimes happens
        if(myPos != bezier.control[3])
        {
          CGAL::insert(*myMap, Curve(myPos, bezier.control[3]));
          updatePos(bezier.control[3]);
        }
      }

    private:
      typedef typename MapTraits::ArrTraits::Curve_2 Curve;

      void
      updatePos(const Point & p)
      {
        myPos = p;
      }

      std::string
      point(const Point & pt)
      {
        std::stringstream ss;
        ss << "(" << CGAL::to_double(pt.x()) << ", " << CGAL::to_double(pt.y())
            << ")";
        return ss.str();
      }

      Map * const myMap;
      Point myPos;
    };

    void
    placePathEdges(const Path & path,
        EdgeDelaunayVertexMapper< MapTraits, VD> * const edgeVertexMapper,
        Map * const map)
    {
      typedef typename EdgeLabeller::EdgeLabels EdgeLabels;
      typedef typename Path::Point PathPoint;

      // Set the edge labels
      EdgeLabeller labeller(map);
      if(path.numEdges() != 0)
      {
        labeller.setEdgeLabels(EdgeLabels(path.leftLabel(), path.rightLabel()));
        const typename Path::Edge & edge = path.edgeFront();
        edgeVertexMapper->setAdjacentDelaunayVertices(edge.leftDelaunayVertex(),
            edge.rightDelaunayVertex());
      }

      const typename Path::Curve & curve = path.curve();
      PathDrawer draw(map);

      if(path.isClosed())
        draw.moveTo(TO_EXACT(curve.vertexBack().end));
      else
        draw.moveTo(TO_EXACT(curve.vertexFront().point()));

      for(size_t i = 0; i < curve.numVertices() - 1; ++i)
      {
        const typename Path::Curve::Vertex & vi = curve.vertex(i);

        if(vi.getBezier())
        {
          const typename MapTraits::Bezier & bezier = toMapType(
              *vi.getBezier());
          labeller.setNextEdgeBezier(bezier);
          draw.curveTo(bezier);
        }
        else
        {
          if(i != 0)
            draw.lineTo(TO_EXACT(vi.point()));
          draw.lineTo(TO_EXACT(vi.end));
        }
      }

      const typename Path::Curve::Vertex & lastVertex = curve.vertexBack();
      if(lastVertex.getBezier())
      {
        const typename MapTraits::Bezier & bezier = toMapType(
            *lastVertex.getBezier());
        labeller.setNextEdgeBezier(bezier);
        draw.curveTo(bezier);
      }
      else
      {
        draw.lineTo(TO_EXACT(lastVertex.point()));
        if(path.isClosed())
          // TODO: Check this, it seems odd that it's the same call again as above
          draw.lineTo(TO_EXACT(lastVertex.point()));
      }
    }

    void
    createBoundary(const PathArrangement & arrangement, Map * const map)
    {
      // Here we perform a counter-clockwise circulation of the infinite
      // vertex.  This corresponds to a clockwise circulation of the convex
      // hull of the Delaunay triangulation.

      typedef VD Voronoi;
      typedef typename Voronoi::Delaunay_graph Delaunay;
      typedef typename Delaunay::Edge Edge;
      typedef typename Delaunay::Face_circulator FaceCirculator;
      typedef typename EdgeLabeller::EdgeLabels EdgeLabels;

      const Voronoi & voronoi = arrangement.getVoronoi();
      const Delaunay & delaunay = voronoi.dual();

      const typename Delaunay::Vertex_handle infiniteVertex =
          delaunay.infinite_vertex();

      FaceCirculator start = delaunay.incident_faces(infiniteVertex);
      FaceCirculator cl = start;
      do
      {
        if(spansBoundary< Delaunay>(Edge(cl, cl->index(infiniteVertex))))
          break;
        ++cl;
      } while(cl != start);
      start = cl; // Start at the newly found boundary edge (or the original start)

      // Set up the labels
      EdgeLabeller labeller(map);
      labeller.setEdgeLabels(
          EdgeLabels(boost::optional< Label>(),
              cl->vertex(delaunay.ccw(cl->index(infiniteVertex)))->info()));

      // Create the drawer
      PathDrawer draw(map);
      draw.moveTo(
          TO_EXACT(
              cl->vertex(delaunay.ccw(cl->index(infiniteVertex)))->point()));

      const typename PathArrangement::BoundaryVerticesConst & boundaryVertices =
          arrangement.getBoundaryVertices();
      typename PathArrangement::BoundaryVerticesConst::const_iterator it;
      do
      {
        const Edge edge(cl, cl->index(infiniteVertex));

        if(spansBoundary< Delaunay>(edge))
        {
          // Viewing a vertical edge with the infinite vertex (i) to its right
          // that span a boundary it looks like this:
          //
          //   |.  <- ccw(i)
          // --|   <- The boundary edge that intersects the convex hull
          //   |.  <- cw(i)
          it = boundaryVertices.find(edge);
          SSLIB_ASSERT(it != boundaryVertices.end());

          const typename Path::Point & p1 =
              it->second.idx() == 0 ?
                  it->second.path()->curve().vertexFront().point() :
                  it->second.path()->curve().vertexBack().point();

          // Insert the edge segment into the arrangement
          draw.lineTo(TO_EXACT(p1));

          labeller.setEdgeLabels(
              EdgeLabels(boost::optional< Label>(),
                  cl->vertex(delaunay.cw(edge.second))->info()));
        }

        draw.lineTo(TO_EXACT(cl->vertex(delaunay.cw(edge.second))->point()));

        ++cl;
      } while(cl != start);
    }

    void
    populateFaceLabels(Map * const map) const
    {
      BOOST_FOREACH(typename Map::Face & f,
          boost::make_iterator_range(map->faces_begin(), map->faces_end()))
      {
        if(f.has_outer_ccb())
        {
          const typename Map::Ccb_halfedge_circulator start = f.outer_ccb();
          typename Map::Ccb_halfedge_circulator cl = start;
//          std::cout << "Start face:\n";
          do
          {
            if(cl->data().label)
            {
              f.data().label = cl->data().label;
              break;
//              std::cout << *f.data().label << "\n";
            }
            ++cl;
          } while(cl != start);
//          std::cout << "End\n\n";
        }
      }
    }

  private:
    typename MapTraits::Bezier
    toMapType(const Bezier & bezier)
    {
      typename MapTraits::Bezier arrBezier;
      arrBezier.alpha = TO_EXACT(bezier.alpha);
      arrBezier.beta = TO_EXACT(bezier.beta);
      for(size_t i = 0; i < 4; ++i)
        arrBezier.control[i] = TO_EXACT(bezier.control[i]);
      return arrBezier;
    }
  };

template< typename MapTraits, typename VD>
  const typename MapBuilder< MapTraits, VD>::IkToEk MapBuilder< MapTraits, VD>::TO_EXACT =
      typename MapBuilder< MapTraits, VD>::IkToEk();

}

template< class VD>
  std::pair< typename VoronoiLabel< VD>::Type, typename VoronoiLabel< VD>::Type>
  getBoundaryPair(const typename VD::Halfedge & he)
  {
    const typename VD::Delaunay_graph::Edge edge = he.dual();
    return std::make_pair(edge.first->vertex((edge.second + 1) % 3)->info(),
        edge.first->vertex((edge.second + 2) % 3)->info());
  }

template< class DG>
  typename BoundaryPair< typename DelaunayLabel< DG>::Type>::Type
  getSpanningPair(const typename DG::Edge edge)
  {
    return typename BoundaryPair< typename DelaunayLabel< DG>::Type>::Type(
        edge.first->vertex((edge.second + 2) % 3)->info(),
        edge.first->vertex((edge.second + 1) % 3)->info());
  }

template< class VD>
  bool
  isBoundary(const typename VD::Halfedge & he)
  {
    return spansBoundary< typename VD::Delaunay_graph>(he.dual());
  }

template< class DG>
  bool
  spansBoundary(const typename DG::Edge & edge)
  {
    return edge.first->vertex((edge.second + 2) % 3)->info()
        != edge.first->vertex((edge.second + 1) % 3)->info();
  }

template< typename VD>
  CGAL::Polygon_2< typename VD::Delaunay_geom_traits>
  delaunayDomain(const typename VD::Vertex_handle & vtx, const VD & voronoi)
  {
    typedef typename VD::Delaunay_graph Delaunay;

    SSLIB_ASSERT(vtx->is_valid());

    const Delaunay & delaunay = voronoi.dual();
    CGAL::Polygon_2< typename VD::Delaunay_geom_traits> poly;

    typename VD::Halfedge_around_vertex_circulator start =
        voronoi.incident_halfedges(vtx);
    typename VD::Halfedge_around_vertex_circulator cl = start;
    typename Delaunay::Edge dgEdge;
    do
    {
      dgEdge = cl->dual();
      poly.push_back(
          dgEdge.first->vertex(delaunay.ccw(dgEdge.second))->point());

      ++cl;
    } while(cl != start);

    return poly;
  }

template< typename MapTraits, typename VD>
  typename MapTraits::Arrangement
  toMap(const VoronoiPathArrangement< VD> & pathArrangement)
  {
    typedef typename MapTraits::Arrangement Map;
    typedef detail::MapBuilder< MapTraits, VD> Builder;

    Map map;
    Builder builder;

    detail::EdgeDelaunayVertexMapper< MapTraits, VD> edgeVertexMapper(&map);

    // Connect the edges
    BOOST_FOREACH(const typename Builder::Path & path,
        boost::make_iterator_range(pathArrangement.pathsBegin(), pathArrangement.pathsEnd()))
      builder.placePathEdges(path, &edgeVertexMapper, &map);

    builder.createBoundary(pathArrangement, &map);
    builder.populateFaceLabels(&map);
    edgeVertexMapper.populateFacePoints(pathArrangement.getVoronoi().dual());

    SSLIB_ASSERT(map.is_valid());

    return map;
  }

}
}

#endif /* VORONOI_PATH_UTILITY_DETAIL_H */
