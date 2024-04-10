use cxx_build::CFG;

#[allow(unused_must_use)]
fn main() {
    CFG.include_prefix = "slippy_map_tiles";
    cxx_build::bridge("lib.rs");
    println!("cargo:rerun-if-changed=src/lib.rs");
}
