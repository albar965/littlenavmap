#ifndef MAPGRAPHICTHREAD_H
#define MAPGRAPHICTHREAD_H

#include "mapgraphic.h"
#include <QObject>
#include <QThread>

class MapGraphicThread : public QThread
{
    Q_OBJECT

public:
    explicit MapGraphicThread(MapGraphic::threadData *tData);

    void run() override;

private:
    const float PI = M_PI;
    const float PI_By_2 = M_PI_2;
    const float PI_Times_2 = 2.0f * M_PI;

    const float cutoffLatitude = 1.484422229745332366961F;            // for web mercator projection

    struct lonLat {
        float longitude;
        float latitude;
    };

    struct tilesXY {
        int x;              // tile index within all tiles
        int y;
        int xt;             // pixel index within tile
        int yt;
        QString id;
    };

    const char idFormat[9] = "%d/%d/%d";
    /**
     * @brief returns longitude + 180° and latitude in radians
     * @param x viewport x coordinate (left = 0)
     * @param y viewport y coordinate (top = 0
     * @param xOrigin viewport x coordinate of earth center
     * @param yOrigin viewport y coordinate of earth center
     * @param radiusSq square of sphereRadiusInPixel
     * @param spin sphereSpin
     * @param tilt sphereTilt
     * @return lonLat in radians, longitude + 180°, latitude as 2.0f when x, y outside earth
     */
    lonLat view2LonLatFrom(int x, int y, int xOrigin, int yOrigin, float radiusSq, float spin, float tilt);

    // inverse Gudermannian function
    // t->t from [0, +/- pi/2] to [0, +/- pi/2] is stretched/mapped to t -> 1/2 * ln(tan(t/2 + pi/4)) from [0, +/- pi/2] to [0, +/- 1.75]
    float stretchWebMercator(float t);

    tilesXY lonLat2TilesXYFrom(lonLat angles, int zoom);

    MapGraphic::threadData* tData;
};

#endif // MAPGRAPHICTHREAD_H
