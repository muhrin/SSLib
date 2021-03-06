/*
 * VoronoiPathTracerTest.cpp
 *
 *  Created on: Jun 19, 2014
 *      Author: Martin Uhrin
 */

// INCLUDES //////////////////////////////////
#include "sslibtest.h"

#include <vector>

#include <spl/analysis/MapArrangementTraits.h>
#include <spl/analysis/VoronoiPathTracer.h>
#include <spl/math/Random.h>

using namespace spl;

BOOST_AUTO_TEST_SUITE(VoronoiPathTracer)

typedef analysis::MapArrangementTraits< int> MapTraits;
typedef analysis::VoronoiPathTracer< MapTraits> Tracer;
typedef MapTraits::Arrangement Map;

BOOST_AUTO_TEST_CASE(SimpleTest)
{
  // SETTINGS
  static const size_t NUM_POINTS = 100;

  // Create a bunch of points at random positions with random labels
  std::vector< Tracer::PointLabel> points;
  for(size_t i = 0; i < NUM_POINTS; ++i)
    points.push_back(
        std::make_pair(
            Tracer::Point(math::randu< double>(), math::randu< double>()),
            math::randu< int>(0, 2)));

  const Map map = Tracer::processPath(points.begin(), points.end());

  BOOST_CHECK(map.is_valid());
}

BOOST_AUTO_TEST_CASE(GridTest)
{
  // SETTINGS
  static const size_t GRID_SIZE = 10;
  static const size_t POINT_TYPES = 3;

  // Create a bunch of points at random positions with random labels
  std::vector< Tracer::PointLabel> points;
  for(size_t i = 0; i < GRID_SIZE; ++i)
  {
    for(size_t j = 0; j < GRID_SIZE; ++j)
    {
      points.push_back(
          std::make_pair(
              Tracer::Point(static_cast< double>(i), static_cast< double>(j)),
              math::randu< int>(0, POINT_TYPES)));
    }
  }

  const Map map = Tracer::processPath(points.begin(), points.end());

  BOOST_CHECK(map.is_valid());
}

BOOST_AUTO_TEST_SUITE_END()
