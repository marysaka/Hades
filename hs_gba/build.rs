use std::{path::PathBuf, env};

fn main() {
    cargo_emit::rerun_if_changed!(
        "libgba/",
    );

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    let build_path = out_path.join("build");
    let build_path = build_path.to_str().unwrap();

    cargo_emit::rustc_link_lib!("gba");
    cargo_emit::rustc_link_search!(build_path => "native");

    meson::build("libgba", build_path);

    bindgen::Builder::default()
        .header("libgba/include/gba.h")
        .clang_arg("-fms-extensions")
        .allowlist_function(r"(gba|channel)_.*")
        .allowlist_type(r"(gba|message|notification|event).*")
        .default_enum_style(bindgen::EnumVariation::Rust { non_exhaustive: true })
        .generate()
        .expect("Unable to generate libgba bindings.")
        .write_to_file("src/bindings.rs")
        .expect("Couldn't write bindings!");
}
