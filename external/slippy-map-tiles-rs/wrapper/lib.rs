use ffi::*;
use geo::{self, Simplify};
use slippy_map_tiles;

// primitives
use geo::{line_string, polygon};

// algorithms
use geo::ConvexHull;

#[cxx::bridge(namespace = "slippy_map_tiles")]
mod ffi {
    #[derive(Clone, Copy)]
    struct Pos {
        x: f64,
        y: f64,
    }

    // Rust types and signatures exposed to C++.
    extern "Rust" {
        fn simplify(input: &Vec<Pos>, epsilon: f64) -> Vec<Pos>;
    }
}

fn simplify(input: &Vec<Pos>, epsilon: f64) -> Vec<Pos> {
    let mut data = Vec::new();

    for val in input {
        data.push(geo::Coord { x: val.x, y: val.y });
    }

    let geo_line_string = geo::LineString::new(data);

    let simplified_geo_line_string = geo_line_string.simplify(&epsilon);

    let mut return_value: Vec<Pos> = Vec::new();

    for value in simplified_geo_line_string {
        return_value.push(Pos {
            x: value.x,
            y: value.y,
        });
    }

    return_value
}
