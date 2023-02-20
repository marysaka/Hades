use std::{env, path::PathBuf};

fn main() {
    cargo_emit::rerun_if_changed!("../libgba/");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    let build_path = out_path.join("build");
    let build_path = build_path.to_str().unwrap();

    cargo_emit::rustc_link_lib!("gba");
    cargo_emit::rustc_link_search!(build_path => "native");

    meson::build("../libgba", build_path);

    bindgen::Builder::default()
        .header("../libgba/include/gba.h")
        .header("../libgba/include/channel/event.h")
        .clang_arg("-I../libgba/include")
        .clang_arg("-fms-extensions")
        .opaque_type(r"(core|scheduler|memory|ppu|apu|io|gpio).*")
        .allowlist_function(r"(gba|channel)_.*")
        .allowlist_type(
            r"(gba|shared_data|channels|message|notification|event|backup_storage_types|keys).*",
        )
        .default_enum_style(bindgen::EnumVariation::Rust {
            non_exhaustive: true,
        })
        .generate()
        .expect("Unable to generate libgba bindings.")
        .write_to_file(out_path.join("libgba_sys.rs"))
        .expect("Failed to write libgba bindings.");
}
