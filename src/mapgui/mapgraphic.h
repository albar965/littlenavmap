#ifndef MAPGRAPHIC_H
#define MAPGRAPHIC_H

#include "marble/AbstractFloatItem.h"
#include "marble/MarbleModel.h"
#include "marble/MarbleWidget.h"
#include "marble/MarbleWidgetInputHandler.h"
#include "marble/ViewportParams.h"
#include "marble/TextureLayer.h"
#include <QSettings>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QXmlStreamReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QImageReader>
#include <QThread>

class MapGraphic : public QWidget
{
    Q_OBJECT

// implemented:

public:
    enum DisplayAs {
        Sphere,             // rectangular raster tiles map gets drawn onto a 3D sphere;
                            // the rectangular raster tiles must stem from a web mercator
                            // projection of earths surface as a 3D sphere onto a 2D rectangle
        Rectangle           // rectangular raster tiles map gets drawn into a rectangle
                            // covering longitudes -180° to +180° and latitudes -90° to +90°,
                            // 1° = 1 pixel for longitude and latitude;
                            // the rectangular raster tiles must stem from a web mercator
                            // projection of earths surface as a 3D sphere onto a 2D rectangle
    };

    struct threadData {
        int width;
        int height;
        QImage *image;
        QHash<QString, QImage> *currentTiles;
        int tileWidth;
        int tileHeight;
        int xStart;
        int xEnd;
        int yStart;
        int yEnd;
        int xOrigin;
        int yOrigin;
        int zoom;           // 0 = whole earth's map = 1 tile, n = whole earth's map = 2^n equi-sized tiles in and per x and y direction
        float spin;
        float tilt;
        float radius;
        bool tryZoomedOut;
        bool rastering;
        int indexLastIdRequested;
        std::list<QString> idsMissing;          // linked list is inherently suited for thread-safety when appropriately implemented and used
        std::list<QString> idsDelivered;
        int indexLastIdCompleted;
        int amountFailed;
    };

private:
    QPainter *painter;
    QNetworkAccessManager *netManager;
    void (MapGraphic::*paintDisplayAs)(QPaintEvent*);

    float tanDefaultLensHalfHorizontalOpeningAngle = 1.732050807F;      // tan(60°); can change after resizing viewport
    const float earthRadius = 6378.137F;                                // Moritz, H. (1980); XVII General Assembly of the IUGG
    //float referenceCoverage;                                          // helps derivation
    //float referenceDistance;                                          // helps derivation

    /**
     * @brief range (0.0f; 1.0f], exclusive border = (, inclusive border = ].
     * Multiplicator to viewport width and height for painting map.
     * Viewport's width's and height's pixels should each be used to resolve the map
     * but high resolution displays have become abundant in mobile (laptop) devices
     * market without those devices processing power being sufficient to display a
     * resolved image on every of their pixels at a frame rate of 60 f/s. (Resolving
     * the map using every pixel is like ray casting.) In order to let the user be
     * able to experience a fluid 60 f/s map interaction, that user should be able
     * to set the scaleFactor.
     */
    float scaleFactor = 1.0f;

    // the following 3 can be static if all MapGraphic shall show the same view on earth
    float sphereSpin = 0.0f;        // spin offset around north/south axis
                                    // (always north/south axis, even when tilted)
    float sphereTilt = 0.0f;        // tilt offset around left/right axis
                                    // (always left/right axis, independant of tilt and spin)
    float sphereRadiusInPixel;      // the number of pixels earth's radius covers / would cover
                                    // when earth painted / painted fully

    float oldSpin = 0.0f;
    float oldTilt = 0.0f;
    float oldZoom;

    int zoomMax;
    int tileWidth;
    int tileHeight;
    QString tileURL;

    bool doPaint = false;

    QHash<QString, QHash<QString, QImage>*> tilesDownloaded;
    QHash<QString, QImage> *currentTiles;
    QHash<QString, bool> failedTiles;
    QHash<QString, QString> request2id;

    QString tileURLForLookup;

    void paintSphere(QPaintEvent *event);
    void paintRectangle(QPaintEvent *event);

protected:
    /**
     * @brief Overridden paintEvent() of QWidget.
     */
    virtual void paintEvent( QPaintEvent *event );

public:
    explicit MapGraphic( QWidget *parent = 0 );
    virtual ~MapGraphic();

public Q_SLOTS:
    void netReplyReceived(QNetworkReply *reply);

// migrated:

public:
    void setProjection( Marble::Projection projection );

    /**
     * @brief Set a new map theme
     * @param maptheme  The ID of the new maptheme. To ensure that a unique
     * identifier is being used the theme does NOT get represented by its
     * name but the by relative location of the file that specifies the theme:
     *
     * Example:
     *    maptheme = "earth/bluemarble/bluemarble.dgml"
     */
    void setMapThemeId( const QString& maptheme );

    /**
     * @brief Return the current distance.
     */
    qreal distance() const;

protected:
    /**
     * @brief Overridden resizeEvent() of QWidget.
     */
    virtual void resizeEvent( QResizeEvent *event );

public Q_SLOTS:
    /**
     * @brief  Set the distance of the observer to the globe in km.
     * @param  distance  The new distance in km.
     */
    void setDistance( qreal distance );

// empty implementation:

public:
    /**
     * @brief Return the model that this view shows.
     */
    Marble::MarbleModel *model();
    const Marble::MarbleModel *model() const;

    /**
     * Summarized render status of the current map view
     * @see renderState
     */
    Marble::RenderStatus renderStatus() const;

    /**
     * @brief  Return a QPixmap with the current contents of the widget.
     */
    QPixmap mapScreenShot();

    /**
     * @brief  Get the Projection used for the map
     * @return @c Spherical         a Globe
     * @return @c Equirectangular   a flat map
     * @return @c Mercator          another flat map
     */
    Marble::Projection projection() const;

    /**
     * @brief Get the earth coordinates corresponding to a pixel in the widget.
     * @param x      the x coordinate of the pixel
     * @param y      the y coordinate of the pixel
     * @param lon    the longitude angle is returned through this parameter
     * @param lat    the latitude angle is returned through this parameter
     * @return @c true  if the pixel (x, y) is within the globe
     *         @c false if the pixel (x, y) is outside the globe, i.e. in space.
     */
    bool geoCoordinates( int x, int y,
                        qreal& lon, qreal& lat,
                        Marble::GeoDataCoordinates::Unit = Marble::GeoDataCoordinates::Degree ) const;

    Marble::ViewportParams *viewport();
    const Marble::ViewportParams *viewport() const;

    /**
     * @brief  Returns the limit in kilobytes of the volatile (in RAM) tile cache.
     * @return the limit of volatile tile cache
     */
    quint64 volatileTileCacheLimit() const;

    /**
     * @brief  Return whether the night shadow is visible.
     * @return visibility of night shadow
     */
    bool showSunShading() const;

    /**
     * @brief Return the current zoom amount.
     */
    int zoom() const;

    int zoomStep() const;

    /**
     * @brief Return the minimum zoom value for the current map theme.
     */
    int maximumZoom() const;

    /**
     * @brief Get the screen coordinates corresponding to geographical coordinates in the widget.
     * @param lon    the lon coordinate of the requested pixel position
     * @param lat    the lat coordinate of the requested pixel position
     * @param x      the x coordinate of the pixel is returned through this parameter
     * @param y      the y coordinate of the pixel is returned through this parameter
     * @return @c true  if the geographical coordinates are visible on the screen
     *         @c false if the geographical coordinates are not visible on the screen
     */
    bool screenCoordinates( qreal lon, qreal lat,
                           qreal& x, qreal& y ) const;

    /**
     * @brief Return the latitude of the center point.
     * @return The latitude of the center point in degree.
     */
    qreal centerLatitude() const;

    /**
     * @brief Return the longitude of the center point.
     * @return The longitude of the center point in degree.
     */
    qreal centerLongitude() const;

    /**
     * @brief  Return whether the coordinate grid is visible.
     * @return The coordinate grid visibility.
     */
    bool showGrid() const;

    /**
     * @brief  Return whether the place marks are visible.
     * @return The place mark visibility.
     */
    bool showPlaces() const;

    /**
     * @brief  Return whether the city place marks are visible.
     * @return The city place mark visibility.
     */
    bool showCities() const;

    /**
     * @brief  Return whether the terrain place marks are visible.
     * @return The terrain place mark visibility.
     */
    bool showTerrain() const;

    /**
     * @brief  Return whether other places are visible.
     * @return The visibility of other places.
     */
    bool showOtherPlaces() const;

    /**
     * @brief  Return whether the ice layer is visible.
     * @return The ice layer visibility.
     */
    bool showIceLayer() const;

    /**
     * @brief Returns a list of all FloatItems on the widget
     * @return the list of the floatItems
     */
    QList<Marble::AbstractFloatItem *> floatItems() const;

    /**
     * @brief Add a layer to be included in rendering.
     */
    void addLayer( Marble::LayerInterface *layer );

    /**
     * @brief Remove a layer from being included in rendering.
     */
    void removeLayer( Marble::LayerInterface *layer );

    /**
     * @brief Return the minimum zoom value for the current map theme.
     */
    int minimumZoom() const;

    /**
     * @brief Retrieve the view context (i.e. still or animated map)
     */
    Marble::ViewContext viewContext() const;

    /**
     * @brief Returns the FloatItem with the given id
     * @return The pointer to the requested floatItem,
     *
     * If no item is found the null pointer is returned.
     */
    Marble::AbstractFloatItem * floatItem( const QString &nameId ) const;

    /**
     * Writes the plugin settings in the passed QSettings.
     * You shouldn't use this in a KDE application as these use KConfig. Here you could
     * use MarblePart which is handling this automatically.
     * @param settings The QSettings object to be used.
     */
    void writePluginSettings( QSettings& settings ) const;

    /**
     * @brief Returns a list of all RenderPlugins on the widget, this includes float items
     * @return the list of RenderPlugins
     */
    QList<Marble::RenderPlugin *> renderPlugins() const;

    /**
     * Reads the plugin settings from the passed QSettings.
     * You shouldn't use this in a KDE application as these use KConfig. Here you could
     * use MarblePart which is handling this automatically.
     * @param settings The QSettings object to be used.
     */
    void readPluginSettings( QSettings& settings );

    /**
     * Returns the current input handler
     */
    Marble::MarbleWidgetInputHandler *inputHandler() const;

public Q_SLOTS:
    /**
     * @brief  Zoom the view to a certain zoomlevel
     * @param  zoom  the new zoom level.
     *
     * The zoom level is an abstract value without physical
     * interpretation.  A zoom value around 1000 lets the viewer see
     * all of the earth in the default window.
     */
    void setZoom( int zoom, Marble::FlyToMode mode = Marble::Instant );

    /**
     * @brief  Zoom the view by a certain step
     * @param  zoomStep  the difference between the old zoom and the new
     */
    void zoomViewBy( int zoomStep, Marble::FlyToMode mode = Marble::Instant );

    /**
     * @brief  Zoom in by the amount zoomStep.
     */
    void zoomIn( Marble::FlyToMode mode = Marble::Automatic );

    /**
     * @brief  Zoom out by the amount zoomStep.
     */
    void zoomOut( Marble::FlyToMode mode = Marble::Automatic );

    /**
     * @brief  Center the view on a geographical point
     * @param  lat  an angle in degrees parallel to the latitude lines
     *              +90(N) - -90(S)
     * @param  lon  an angle in degrees parallel to the longitude lines
     *              +180(W) - -180(E)
     */
    void centerOn( const qreal lon, const qreal lat, bool animated = false );

    /**
     * @brief Center the view on a bounding box so that it completely fills the viewport
     * This method not only centers on the center of the GeoDataLatLon box but it also
     * adjusts the zoom of the marble widget so that the LatLon box provided fills
     * the viewport.
     * @param box The GeoDataLatLonBox to zoom and move the MarbleWidget to.
     */
    void centerOn( const Marble::GeoDataLatLonBox& box, bool animated = false );

    /**
     * @brief  Move down by the moveStep.
     */
    void moveDown( Marble::FlyToMode mode = Marble::Automatic );

    /**
     * @brief  Move left by the moveStep.
     */
    void moveLeft( Marble::FlyToMode mode = Marble::Automatic );

    /**
     * @brief  Move right by the moveStep.
     */
    void moveRight( Marble::FlyToMode mode = Marble::Automatic );

    /**
     * @brief  Move up by the moveStep.
     */
    void moveUp( Marble::FlyToMode mode = Marble::Automatic );

    /* Set API key which will be used as a replacement for {apikey} in storageLayout of type "Custom"/> */
    void setKeys(QHash<QString, QString> keys);

    /**
     * @brief  Set whether the cloud cover is visible
     * @param  visible  visibility of the cloud cover
     */
    void setShowClouds( bool visible );

    /**
     * @brief  Set whether the night shadow is visible.
     * @param  visibile visibility of shadow
     */
    void setShowSunShading( bool visible );
    void setSunShadingDimFactor(qreal dimFactor);

    /**
     * @brief  Set whether the coordinate grid overlay is visible
     * @param  visible  visibility of the coordinate grid
     */
    void setShowGrid( bool visible );

    /**
     * @brief  Set whether the place mark overlay is visible
     * @param  visible  visibility of the place marks
     */
    void setShowPlaces( bool visible );

    /**
     * @brief  Set whether the city place mark overlay is visible
     * @param  visible  visibility of the city place marks
     */
    void setShowCities( bool visible );

    /**
     * @brief  Set whether the terrain place mark overlay is visible
     * @param  visible  visibility of the terrain place marks
     */
    void setShowTerrain( bool visible );

    /**
     * @brief  Set whether the other places overlay is visible
     * @param  visible  visibility of other places
     */
    void setShowOtherPlaces( bool visible );

    /**
     * @brief  Set whether the ice layer is visible
     * @param  visible  visibility of the ice layer
     */
    void setShowIceLayer( bool visible );

    /**
     * @brief Set the map quality for the specified view context.
     *
     * @param quality map quality for the specified view context
     * @param viewContext view context whose map quality should be set
     */
    void setMapQualityForViewContext( Marble::MapQuality quality, Marble::ViewContext viewContext );

    /**
     * @brief Set the view context (i.e. still or animated map)
     */
    void setViewContext( Marble::ViewContext viewContext );

    /**
     * @brief Set whether travels to a point should get animated
     */
    void setAnimationsEnabled( bool enabled );

    /**
     * @brief  Set the limit of the volatile (in RAM) tile cache.
     * @param  kilobytes The limit in kilobytes.
     */
    void setVolatileTileCacheLimit( quint64 kiloBytes );

    /**
     * @brief  Sets the value of a map theme property
     * @param  value  value of the property (usually: visibility)
     *
     * Later on we might add a "setPropertyType and a QVariant
     * if needed.
     */
    void setPropertyValue( const QString& name, bool value );

    void clearVolatileTileCache();

    Marble::TextureLayer *textureLayer() const;

    /**
     * @brief  Set the radius of the globe in pixels.
     * @param  radius  The new globe radius value in pixels.
     */
    void setRadius( int radius );

    /**
     * @deprecated To be removed soon. Please use setZoom instead. Same parameters.
     */
    void zoomView( int zoom, Marble::FlyToMode mode = Marble::Instant );

    /**
     * @brief  Rotate the view by the two angles phi and theta.
     * @param  deltaLon  an angle that specifies the change in terms of longitude
     * @param  deltaLat  an angle that specifies the change in terms of latitude
     *
     * This function rotates the view by two angles,
     * deltaLon ("theta") and deltaLat ("phi").
     * If we start at (0, 0), the result will be the exact equivalent
     * of (lon, lat), otherwise the resulting angle will be the sum of
     * the previous position and the two offsets.
     */
    void rotateBy( const qreal deltaLon, const qreal deltaLat, Marble::FlyToMode mode = Marble::Instant );

    /**
     * @brief  Center the view on a point
     * This method centers the Marble map on the point described by the latitude
     * and longitude in the GeoDataCoordinate parameter @c point. It also zooms
     * the map to be at the elevation described by the altitude. If this is
     * not the desired functionality or you do not have an accurate altitude
     * then use @see centerOn(qreal, qreal, bool)
     * @param point the point in 3 dimensions above the globe to move the view
     *              to. It will always be looking vertically down.
     */
    void centerOn( const Marble::GeoDataCoordinates &point, bool animated = false );

    /**
     * @brief Center the view on a placemark according to the following logic:
     * - if the placemark has a lookAt, zoom and center on that lookAt
     * - otherwise use the placemark geometry's latLonAltBox
     * @param box The GeoDataPlacemark to zoom and move the MarbleWidget to.
     */
    void centerOn( const Marble::GeoDataPlacemark& placemark, bool animated = false );

    /**
     * @brief  Set the latitude for the center point
     * @param  lat  the new value for the latitude in degree.
     * @param  mode the FlyToMode that will be used.
     */
    void setCenterLatitude( qreal lat, Marble::FlyToMode mode = Marble::Instant );

    /**
     * @brief  Set the longitude for the center point
     * @param  lon  the new value for the longitude in degree.
     * @param  mode the FlyToMode that will be used.
     */
    void setCenterLongitude( qreal lon, Marble::FlyToMode mode = Marble::Instant );

    /**
     * @brief Center the view on the default start point with the default zoom.
     */
    void goHome( Marble::FlyToMode mode = Marble::Automatic );

    /**
      * @brief Change the camera position to the given position.
      * @param lookAt New camera position. Changing the camera position means
      * that both the current center position as well as the zoom value may change
      * @param mode Interpolation type for intermediate camera positions. Automatic
      * (default) chooses a suitable interpolation among Instant, Lenar and Jump.
      * Instant will directly set the new zoom and position values, while
      * Linear results in a linear interpolation of intermediate center coordinates
      * along the sphere and a linear interpolation of changes in the camera distance
      * to the ground. Finally, Jump will behave the same as Linear with regard to
      * the center position interpolation, but use a parabolic height increase
      * towards the middle point of the intermediate positions. This appears
      * like a jump of the camera.
      */
    void flyTo( const Marble::GeoDataLookAt &lookAt, Marble::FlyToMode mode = Marble::Automatic );

    /**
     * @brief  Set the Projection used for the map
     * @param  projection projection type (e.g. Spherical, Equirectangular, Mercator)
     */
    void setProjection( int projection );

    /**
     * @brief  Set whether the overview map overlay is visible
     * @param  visible  visibility of the overview map
     */
    void setShowOverviewMap( bool visible );

    /**
     * @brief  Set whether the scale bar overlay is visible
     * @param  visible  visibility of the scale bar
     */
    void setShowScaleBar( bool visible );

    /**
     * @brief  Set whether the compass overlay is visible
     * @param  visible  visibility of the compass
     */
    void setShowCompass( bool visible );

    /**
     * @brief  Set whether city lights instead of night shadow are visible.
     * @param  visible visibility of city lights
     */
    void setShowCityLights( bool visible );

    /**
     * @brief  Set the globe locked to the sub solar point
     * @param  vsible if globe is locked to the sub solar point
     */
    void setLockToSubSolarPoint( bool visible );

    /**
     * @brief  Set whether the sun icon is shown in the sub solar point
     * @param  visible if the sun icon is shown in the sub solar point
     */
    void setSubSolarPointIconVisible( bool visible );

    /**
     * @brief  Set whether the atmospheric glow is visible
     * @param  visible  visibility of the atmospheric glow
     */
    void setShowAtmosphere( bool visible );

    /**
     * @brief  Set whether the crosshairs are visible
     * @param  visible  visibility of the crosshairs
     */
    void setShowCrosshairs( bool visible );

    /**
     * @brief  Set whether the relief is visible
     * @param  visible  visibility of the relief
     */
    void setShowRelief( bool visible );

    /**
     * @brief  Set whether the borders visible
     * @param  visible  visibility of the borders
     */
    void setShowBorders( bool visible );

    /**
     * @brief  Set whether the rivers are visible
     * @param  visible  visibility of the rivers
     */
    void setShowRivers( bool visible );

    /**
     * @brief  Set whether the lakes are visible
     * @param  visible  visibility of the lakes
     */
    void setShowLakes( bool visible );

    /**
     * @brief Set whether the frame rate gets shown
     * @param visible  visibility of the frame rate
     */
    void setShowFrameRate( bool visible );

    void setShowBackground( bool visible );

    /**
     * @brief Set whether the is tile is visible
     *
     *
     * NOTE: This is part of the transitional debug API
     *       and might be subject to changes until Marble 0.8
     * @param visible visibility of the tile
     */
    void setShowTileId( bool visible );

    /**
     * @brief Set whether the runtime tracing for layers gets shown
     * @param visible visibility of the runtime tracing
     */
    void setShowRuntimeTrace( bool visible );

    bool showRuntimeTrace() const;

    /**
     * @brief Set whether to enter the debug mode for
     * polygon node drawing
     * @param visible visibility of the node debug mode
     */
    void setShowDebugPolygons( bool visible);

    bool showDebugPolygons() const;

    /**
     * @brief A slot that is called when the model starts to create new tiles.
     * @param creator the tile creator object.
     * @param name  the name of the created theme.
     * @param description  a descriptive text that can be shown in a dialog.
     * @see    creatingTilesProgress
     *
     * This function is connected to the models signal with the same
     * name.  When the model needs to create a cache of tiles in
     * several different resolutions, it will emit creatingTilesStart
     * once with a name of the theme and a descriptive text.  The
     * widget can then pop up a dialog to explain why there is a
     * delay.  The model will then call creatingTilesProgress several
     * times until the parameter reaches 100 (100%), after which the
     * creation process is finished.  After this there will be no more
     * calls to creatingTilesProgress, and the poup dialog can then be
     * closed.
     */
    void creatingTilesStart( Marble::TileCreator *creator, const QString& name, const QString& description );

    /**
     * @brief Re-download all visible tiles.
     */
    void reloadMap();

    void downloadRegion( QVector<Marble::TileCoordsPyramid> const & );

    /**
     * @brief Used to notify about the position of the mouse click
      */
    void notifyMouseClick( int x, int y );

    void setSelection( const QRect& region );

    void setInputEnabled( bool );

Q_SIGNALS:

protected:
    virtual void connectNotify(const QMetaMethod &signal);

    virtual void disconnectNotify(const QMetaMethod &signal);

    /**
     * @brief Reimplementation of the leaveEvent() function in QWidget.
     */
    virtual void leaveEvent( QEvent *event );

    /**
      * @brief Reimplementation of the changeEvent() function in QWidget to
      * react to changes of the enabled state
      */
    virtual void changeEvent( QEvent * event );

    /**
     * @brief Enables custom drawing onto the MarbleWidget straight after
     * @brief the globe and before all other layers has been rendered.
     * @param painter
     *
     * @deprecated implement LayerInterface and add it using @p addLayer()
     */
    virtual void customPaint( Marble::GeoPainter *painter );

// not implemented:

public:

    /**
     * @brief Set the input handler
     */
    void setInputHandler( Marble::MarbleWidgetInputHandler *handler );

    /**
     * @brief Get the GeoSceneDocument object of the current map theme
     */
    Marble::GeoSceneDocument * mapTheme() const;

    /**
     * @brief Returns all widgets of dataPlugins on the position curpos
     */
    QList<Marble::AbstractDataPluginItem *> whichItemAt( const QPoint& curpos ) const;

    Marble::PopupLayer* popupLayer();

    /**
     * @brief Get the ID of the current map theme
     * To ensure that a unique identifier is being used the theme does NOT
     * get represented by its name but the by relative location of the file
     * that specifies the theme:
     *
     * Example:
     *    mapThemeId = "earth/bluemarble/bluemarble.dgml"
     */
    QString mapThemeId() const;

    /**
     * @brief Return the projected region which describes the (shape of the) projected surface.
     */
    QRegion mapRegion() const;

    /**
     * @brief  Return the radius of the globe in pixels.
     */
    int radius() const;

    int tileZoomLevel() const;

    /**
     * @brief Return the current distance string.
     */
    QString distanceString() const;

    /**
     * @brief  Return how much the map will move if one of the move slots are called.
     * @return The move step.
     */
    qreal moveStep() const;

    /**
    * @brief Return the lookAt
    */
    Marble::GeoDataLookAt lookAt() const;

    /**
     * @return The current point of focus, e.g. the point that is not moved
     * when changing the zoom level. If not set, it defaults to the
     * center point.
     * @see centerLongitude centerLatitude setFocusPoint resetFocusPoint
     */
    Marble::GeoDataCoordinates focusPoint() const;

    /**
     * @brief Change the point of focus, overridding any previously set focus point.
     * @param focusPoint New focus point
     * @see focusPoint resetFocusPoint
     */
    void setFocusPoint( const Marble::GeoDataCoordinates &focusPoint );

    /**
     * @brief Invalidate any focus point set with @ref setFocusPoint.
     * @see focusPoint setFocusPoint
     */
    void resetFocusPoint();

    /**
      * @brief Return the globe radius (pixel) for the given distance (km)
      */
    qreal radiusFromDistance( qreal distance ) const;

    /**
      * @brief Return the distance (km) at the given globe radius (pixel)
      */
    qreal distanceFromRadius( qreal radius ) const;

    /**
      * Returns the zoom value (no unit) corresponding to the given camera distance (km)
      */
    qreal zoomFromDistance( qreal distance ) const;

    /**
      * Returns the distance (km) corresponding to the given zoom value
      */
    qreal distanceFromZoom( qreal zoom ) const;

    QVector<const Marble::GeoDataFeature *> whichFeatureAt( const QPoint& ) const;

    /**
     * @brief  Return whether the overview map is visible.
     * @return The overview map visibility.
     */
    bool showOverviewMap() const;

    /**
     * @brief  Return whether the scale bar is visible.
     * @return The scale bar visibility.
     */
    bool showScaleBar() const;

    /**
     * @brief  Return whether the compass bar is visible.
     * @return The compass visibility.
     */
    bool showCompass() const;

    /**
     * @brief  Return whether the cloud cover is visible.
     * @return The cloud cover visibility.
     */
    bool showClouds() const;

    /**
     * @brief  Return whether the city lights are shown instead of the night shadow.
     * @return visibility of city lights
     */
    bool showCityLights() const;

    /**
     * @brief  Return whether the globe is locked to the sub solar point
     * @return if globe is locked to sub solar point
     */
    bool isLockedToSubSolarPoint() const;

    /**
     * @brief  Return whether the sun icon is shown in the sub solar point.
     * @return visibility of the sun icon in the sub solar point
     */
    bool isSubSolarPointIconVisible() const;

    /**
     * @brief  Return whether the atmospheric glow is visible.
     * @return The cloud cover visibility.
     */
    bool showAtmosphere() const;

    /**
     * @brief  Return whether the crosshairs are visible.
     * @return The crosshairs' visibility.
     */
    bool showCrosshairs() const;

    /**
     * @brief  Return whether the relief is visible.
     * @return The relief visibility.
     */
    bool showRelief() const;

    /**
     * @brief  Return whether the borders are visible.
     * @return The border visibility.
     */
    bool showBorders() const;

    /**
     * @brief  Return whether the rivers are visible.
     * @return The rivers' visibility.
     */
    bool showRivers() const;

    /**
     * @brief  Return whether the lakes are visible.
     * @return The lakes' visibility.
     */
    bool showLakes() const;

    /**
     * @brief  Return whether the frame rate gets displayed.
     * @return the frame rates visibility
     */
    bool showFrameRate() const;

    bool showBackground() const;

    /**
     * @brief Retrieve the map quality depending on the view context
     */
    Marble::MapQuality mapQuality( Marble::ViewContext = Marble::Still ) const;

    /**
     * @brief Retrieve whether travels to a point should get animated
     */
    bool animationsEnabled() const;

    Marble::AngleUnit defaultAngleUnit() const;
    void setDefaultAngleUnit( Marble::AngleUnit angleUnit );

    QFont defaultFont() const;
    void setDefaultFont( const QFont& font );

    /**
     * Detailed render status of the current map view
     */
    Marble::RenderState renderState() const;

    /**
     * Toggle whether regions are highlighted when user selects them
     */
    void setHighlightEnabled( bool enabled );

Q_SIGNALS:
    /**
     * @brief Signal that the zoom has changed, and to what.
     * @param zoom  The new zoom value.
     * @see  setZoom()
     */
    void zoomChanged( int zoom );
    void distanceChanged( const QString& distanceString );

    void tileLevelChanged( int level );

    void viewContextChanged(Marble::ViewContext newViewContext);

    /**
     * @brief Signal that the theme has changed
     * @param theme  Name of the new theme.
     */
    void themeChanged( const QString& theme );

    void projectionChanged( Marble::Projection );

    void mouseMoveGeoPosition( const QString& );

    void mouseClickGeoPosition( qreal lon, qreal lat, Marble::GeoDataCoordinates::Unit );

    void framesPerSecond( qreal fps );

    /** This signal is emit when a new rectangle region is selected over the map
     *  The list of double values include coordinates in degrees using this order:
     *  lon1, lat1, lon2, lat2 (or West, North, East, South) as left/top, right/bottom rectangle.
     */
    void regionSelected( const QList<double>& );

    /**
     * This signal is emit when the settings of a plugin changed.
     */
    void pluginSettingsChanged();

    /**
     * @brief Signal that a render item has been initialized
     */
    void renderPluginInitialized( Marble::RenderPlugin *renderPlugin );

    /**
     * This signal is emitted when the visible region of the map changes. This typically happens
     * when the user moves the map around or zooms.
     */
    void visibleLatLonAltBoxChanged( const Marble::GeoDataLatLonAltBox& visibleLatLonAltBox );

    /**
     * @brief Emitted when the layer rendering status has changed
     * @param status New render status
     */
    void renderStatusChanged( Marble::RenderStatus status );

    void renderStateChanged( const Marble::RenderState &state );

    void highlightedPlacemarksChanged( qreal lon, qreal lat, Marble::GeoDataCoordinates::Unit unit );
};

#endif // MAPGRAPHIC_H
