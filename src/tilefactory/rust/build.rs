use cxx_build::CFG;

#[allow(unused_must_use)]
fn main() {
    CFG.include_prefix = "tilefactory_rust";
    cxx_build::bridge("lib.rs");
    println!("cargo:rerun-if-changed=lib.rs");
}
