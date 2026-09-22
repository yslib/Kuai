use super::*;
use std::fs;

fn options(paths: Vec<PathBuf>, exclude: Vec<PathBuf>) -> FormatArgs {
    FormatArgs {
        check: true,
        paths,
        exclude,
    }
}

fn fixture(root: &Path, relative: &str) -> PathBuf {
    let path = root.join(relative);
    fs::create_dir_all(path.parent().unwrap()).unwrap();
    fs::write(&path, "int x;\n").unwrap();
    fs::canonicalize(path).unwrap()
}

#[test]
fn selection_sorts_deduplicates_and_keeps_all_supported_extensions() {
    let temp = tempfile::tempdir().unwrap();
    let mut expected: Vec<_> = [
        "c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx", "cu", "cuh",
    ]
    .iter()
    .map(|ext| fixture(temp.path(), &format!("nested/source.{ext}")))
    .collect();
    fixture(temp.path(), "notes.rs");
    expected.sort();
    let args = options(
        vec![
            temp.path().into(),
            expected[0].clone(),
            temp.path().join("nested"),
        ],
        vec![],
    );
    assert_eq!(collect_files(temp.path(), &args).unwrap(), expected);
}

#[test]
fn default_selection_is_root_relative() {
    let temp = tempfile::tempdir().unwrap();
    let mut expected: Vec<_> = ["src", "vendor", "python/bindings"]
        .iter()
        .map(|dir| fixture(temp.path(), &format!("kurt-cpp/{dir}/source.cpp")))
        .collect();
    fixture(temp.path(), "unrelated/source.cpp");
    expected.sort();
    assert_eq!(
        collect_files(temp.path(), &options(vec![], vec![])).unwrap(),
        expected
    );
}

#[test]
fn exclusions_match_components_not_string_prefixes() {
    let temp = tempfile::tempdir().unwrap();
    fixture(temp.path(), "skip/a.cpp");
    let kept = fixture(temp.path(), "skip-sibling/a.cpp");
    let args = options(vec![temp.path().into()], vec![temp.path().join("skip")]);
    assert_eq!(collect_files(temp.path(), &args).unwrap(), [kept]);
}

#[test]
fn generated_directories_are_pruned_even_as_roots_but_explicit_files_work() {
    let temp = tempfile::tempdir().unwrap();
    for dir in ["build", "cmake-build-debug", ".git", "__pycache__"] {
        let file = fixture(temp.path(), &format!("{dir}/a.cpp"));
        assert!(
            collect_files(temp.path(), &options(vec![temp.path().join(dir)], vec![]))
                .unwrap()
                .is_empty()
        );
        assert_eq!(
            collect_files(temp.path(), &options(vec![file.clone()], vec![])).unwrap(),
            [file]
        );
    }
}

#[test]
fn missing_targets_and_exclusions_are_errors_even_with_empty_selection() {
    let temp = tempfile::tempdir().unwrap();
    for args in [
        options(vec![temp.path().join("missing")], vec![]),
        options(vec![temp.path().into()], vec![temp.path().join("missing")]),
    ] {
        assert!(
            collect_files(temp.path(), &args)
                .unwrap_err()
                .contains("missing")
        );
    }
}

#[cfg(unix)]
#[test]
fn recursive_symlinks_are_skipped_and_explicit_links_resolve() {
    use std::os::unix::fs::symlink;
    let temp = tempfile::tempdir().unwrap();
    let kept = fixture(temp.path(), "sources/a.cpp");
    let external = fixture(temp.path(), "elsewhere/b.cpp");
    symlink(temp.path(), temp.path().join("sources/cycle")).unwrap();
    let link = temp.path().join("sources/link.cpp");
    symlink(&external, &link).unwrap();
    assert_eq!(
        collect_files(
            temp.path(),
            &options(vec![temp.path().join("sources")], vec![])
        )
        .unwrap(),
        [kept]
    );
    assert_eq!(
        collect_files(temp.path(), &options(vec![link], vec![])).unwrap(),
        [external]
    );
}

#[cfg(unix)]
#[test]
fn explicit_special_files_are_rejected() {
    let temp = tempfile::tempdir().unwrap();
    let socket = temp.path().join("socket.cpp");
    let _listener = std::os::unix::net::UnixListener::bind(&socket).unwrap();
    assert!(collect_files(temp.path(), &options(vec![socket], vec![])).is_err());
}

#[test]
fn versions_require_three_ascii_numeric_components() {
    assert_eq!(parse_version(" 21.1.8\n").unwrap(), "21.1.8");
    for value in [
        "",
        "21",
        "21.1",
        "21.1.8.1",
        "21..8",
        "21.1.x",
        "21.1.8 --help",
        "２１.1.8",
    ] {
        assert!(parse_version(value).is_err(), "accepted {value:?}");
    }
}

#[test]
fn actual_version_must_match_the_pin() {
    for ending in ["", "\n", "\r\n"] {
        verify_version(
            "21.1.8",
            format!("clang-format version 21.1.8{ending}").as_bytes(),
        )
        .unwrap();
    }
    for value in [
        "clang-format version 20.1.0",
        "Apple clang-format version 21.1.8",
        "clang-format version 21.1.8 extra",
    ] {
        assert!(verify_version("21.1.8", value.as_bytes()).is_err());
    }
}

#[test]
fn configuration_requires_valid_version_and_style_files() {
    let temp = tempfile::tempdir().unwrap();
    fs::create_dir(temp.path().join("kurt-cpp")).unwrap();
    let version = temp.path().join("kurt-cpp/.clang-format-version");
    let style = temp.path().join("kurt-cpp/.clang-format");
    assert!(Formatter::load(temp.path()).is_err());
    fs::write(&version, "not a version").unwrap();
    fs::write(&style, "BasedOnStyle: LLVM\n").unwrap();
    assert!(Formatter::load(temp.path()).is_err());
    fs::write(&version, "21.1.8\n").unwrap();
    let formatter = Formatter::load(temp.path()).unwrap();
    assert_eq!(formatter.version, "21.1.8");
    assert_eq!(formatter.style, fs::canonicalize(&style).unwrap());
    fs::remove_file(&style).unwrap();
    assert!(Formatter::load(temp.path()).is_err());
    fs::create_dir(&style).unwrap();
    assert!(Formatter::load(temp.path()).is_err());
}

#[test]
fn formatter_arguments_pin_package_style_and_mode() {
    let formatter = Formatter {
        version: "21.1.8".into(),
        style: PathBuf::from("/source with spaces/.clang-format"),
    };
    assert_eq!(
        formatter.arguments(true),
        [
            "--from",
            "clang-format==21.1.8",
            "clang-format",
            "--style=file:/source with spaces/.clang-format",
            "--dry-run",
            "--Werror"
        ]
        .map(OsString::from)
    );
    assert_eq!(
        formatter.arguments(false),
        [
            "--from",
            "clang-format==21.1.8",
            "clang-format",
            "--style=file:/source with spaces/.clang-format",
            "-i"
        ]
        .map(OsString::from)
    );
}

#[cfg(unix)]
#[test]
fn non_utf8_style_paths_remain_native_arguments() {
    use std::os::unix::ffi::{OsStrExt, OsStringExt};
    let formatter = Formatter {
        version: "21.1.8".into(),
        style: PathBuf::from(OsString::from_vec(b"/source-\xff/.clang-format".to_vec())),
    };
    assert_eq!(
        formatter.arguments(true)[3].as_bytes(),
        b"--style=file:/source-\xff/.clang-format"
    );
}

#[test]
fn batches_obey_total_argument_budget_without_omitting_files() {
    let files: Vec<_> = (0..8)
        .map(|i| PathBuf::from(format!("/some long path/{i}.cpp")))
        .collect();
    let fixed = 100;
    let budget = fixed + 3 * argument_cost(files[0].as_os_str());
    assert!(argument_cost(files[0].as_os_str()) > files[0].as_os_str().len());
    let chunks = batches(&files, fixed, budget).unwrap();
    assert_eq!(chunks.len(), 3);
    assert_eq!(
        chunks
            .iter()
            .flat_map(|chunk| chunk.iter())
            .collect::<Vec<_>>(),
        files.iter().collect::<Vec<_>>()
    );
    for chunk in chunks {
        assert!(
            fixed
                + chunk
                    .iter()
                    .map(|p| argument_cost(p.as_os_str()))
                    .sum::<usize>()
                <= budget
        );
    }
    assert!(batches(&files, fixed, fixed).is_err());
    assert!(batches(&files, budget + 1, budget).is_err());
}
