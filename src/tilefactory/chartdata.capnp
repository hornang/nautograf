@0xcfa67771551e48ef;

struct ChartData {
    name @0: Text;
    nativeScale @1: Int32;
    coverage @2 :List(CoverageArea);
    landAreas @3 :List(LandArea);
    builtUpAreas @4 :List(BuiltUpArea);
    builtUpPoints @5 :List(BuiltUpPoint);
    depthAreas @6 :List(DepthArea);
    soundings @7 :List(Sounding);
    beacons @8 :List(Beacon);
    underwaterRocks @9 :List(UnderwaterRock);
    roads @10 :List(Road);
    lateralBuoys @11 :List(BuoyLateral);
    landRegions @12 :List(LandRegion);
    topLeft @13: Position;
    bottomRight @14: Position;
    coastLines @15: List(CoastLine);
    pontoons @16: List(Pontoon);
    depthContours @17: List(DepthContour);
    shorelineConstructions @18: List(ShorelineConstruction);

    struct CoverageArea {
        polygons @0 :List(Polygon);
    }

    struct LandArea {
        name @0: Text;
        polygons @1 :List(Polygon);
        centroid @2: Position;
    }

    struct BuiltUpArea {
        name @0: Text;
        polygons @1 :List(Polygon);
        centroid @2: Position;
    }

    struct CoastLine {
        lines @0 :List(Line);
    }

    struct DepthContour {
        lines @0 :List(Line);
    }

    struct BuiltUpPoint {
        name @0: Text;
        position @1: Position;
    }

    struct LandRegion {
        name @0: Text;
        position @1: Position;
    }

    struct DepthArea {
        depth @0 :Float32;
        polygons @1 :List(Polygon);
    }

    struct Polygon {
        main @0 :List(Position);
        holes @1 :List(List(Position));
    }

    struct Pontoon {
        name @0: Text;
        polygons @1 :List(Polygon);
        lines @2 :List(Line);
    }

    struct ShorelineConstruction {
        name @0: Text;
        polygons @1 :List(Polygon);
        lines @2 :List(Line);
    }

    struct Position {
        latitude @0: Float64;
        longitude @1: Float64;
    }

    struct Sounding {
        depth @0 :Float32;
        position @1: Position;
    }

    struct Beacon {
        position @0: Position;
        shape @1: BeaconShape;
        name @2: Text;
    }

    struct BuoyLateral {
        position @0: Position;
        category @1: CategoryOfLateralMark;
        shape @2: BuoyShape;
        name @3: Text;
        color @4: Color;
    }

    enum BuoyShape {
        unknown @0;
        conical @1;
        can @2;
        spherical @3;
        pillar @4;
        spar @5;
        barrel @6;
        superBuoy @7;
        iceBuoy @8;
    }

    enum Color {
        unknown @0;
        white @1;
        black @2;
        red @3;
        green @4;
        blue @5;
        yellow @6;
        grey @7;
        brown @8;
        amber @9;
        violet @10;
        orange @11;
        magenta @12;
        pink @13;
    }

    enum CategoryOfLateralMark {
        unknown @0;
        port @1;
        starboard @2;
        channelToStarboard @3;
        channelToPort @4;
    }

    enum BeaconShape {
        unknown @0;
        stake @1;
        withy @2;
        beaconTower @3;
        latticeBeacon @4;
        pileBeacon @5;
        cairn @6;
        boyant @7;
    }

    enum WaterLevelEffect {
        unknown @0;
        partlySubmergedAtHighWater @1;
        alwaysDry @2;
        alwaysSubmerged @3;
        coversAndUncovers @4;
        awash @5;
        subjectToFlooding @6;
        floating @7;
    }

    enum CategoryOfRoad {
        unknown @0;
        motorway @1;
        majorRoad @2;
        minorRoad @3;
        track @4;
        majorStreet @5;
        minorStreet @6;
        crossing @7;
    }

    struct UnderwaterRock {
        position @0: Position;
        depth  @1: Float32;
        waterlevelEffect @2: WaterLevelEffect;
    }

    struct Depth {
        depth @0 :Float32;
        position @1: Position;
    }

    struct Line {
        positions @0 :List(Position);
    }

    struct Road {
        name @0: Text;
        category @1: CategoryOfRoad;
        lines @2 :List(Line);
        polygons @3 :List(Polygon);
    }
}
