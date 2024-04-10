use ffi::*;
use geo::{self, Simplify};

#[cxx::bridge(namespace = "tilefactory_rust")]
mod ffi {
    #[derive(Clone, Copy)]
    struct Pos {
        x: f64,
        y: f64,
    }

    extern "Rust" {
        fn simplify(input: &Vec<Pos>, epsilon: f64) -> Vec<Pos>;
    }
}

fn simplify(input: &Vec<Pos>, epsilon: f64) -> Vec<Pos> {
    let mut coord_vector = Vec::new();

    for val in input {
        coord_vector.push(geo::Coord { x: val.x, y: val.y });
    }

    let geo_line_string = geo::LineString::new(coord_vector);
    let simplified_geo_line_string = geo_line_string.simplify(&epsilon);
    let mut simplified_coord_vector: Vec<Pos> = Vec::new();

    for value in simplified_geo_line_string {
        simplified_coord_vector.push(Pos {
            x: value.x,
            y: value.y,
        });
    }

    simplified_coord_vector
}
