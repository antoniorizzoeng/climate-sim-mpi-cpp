import pytest
import sys
from types import SimpleNamespace
import visualization.cli as cli

def run_cmd(monkeypatch, argv, expected_func):
    captured = {}

    def fake_func(args):
        captured["args"] = args

    monkeypatch.setattr(cli, expected_func.__name__, fake_func)
    parser = cli.build_parser()
    args = parser.parse_args(argv)
    args.func(args)
    return captured["args"]

def make_animate_args(**overrides):
    base = dict(
        dir="outputs",
        var="u",
        steps=None,
        start=None,
        end=None,
        stride=None,
        interval=100,
        fps=10,
        no_repeat=False,
        cmap="viridis",
        vmin=None,
        vmax=None,
        save="anim.gif",
        writer="pillow",
        title_prefix="timestep",
        overlay_minmax=False,
        show_meta=False,
    )
    base.update(overrides)
    return SimpleNamespace(**base)

def make_show_args(**overrides):
    base = dict(
        dir="outputs",
        step=None,
        var="u",
        title=None,
        cmap="viridis",
        vmin=None,
        vmax=None,
        show=False,
        save=None,
        overlay_minmax=False,
        show_meta=False,
    )
    base.update(overrides)
    return SimpleNamespace(**base)

def make_compare_args(**overrides):
    base = dict(
        dir_a="a",
        dir_b="b",
        step=0,
        var_a="u",
        var_b="u",
        title_a=None,
        title_b=None,
        cmap="viridis",
        vmin=None,
        vmax=None,
        no_diff=False,
        diff_cmap="coolwarm",
        diff_vlim=None,
        show=False,
        save=None,
        overlay_minmax=False,
        show_meta=False,
    )
    base.update(overrides)
    return SimpleNamespace(**base)

def test_parse_steps_none_returns_all():
    assert cli._parse_steps_arg(None, [1, 2, 3]) == [1, 2, 3]

def test_parse_steps_single_value():
    assert cli._parse_steps_arg("5", list(range(10))) == [5]

def test_parse_steps_comma_separated():
    assert cli._parse_steps_arg("1,3,5", list(range(10))) == [1, 3, 5]

def test_parse_steps_range_full():
    avail = list(range(10))
    assert cli._parse_steps_arg("2-5", avail) == [2, 3, 4, 5]

def test_parse_steps_open_start_end():
    avail = list(range(5, 15))
    assert cli._parse_steps_arg("-8", avail) == [5, 6, 7, 8]
    assert cli._parse_steps_arg("12-", avail) == [12, 13, 14]

def test_parse_steps_empty_string():
    assert cli._parse_steps_arg("", [1, 2, 3]) == []

def test_cmd_show_no_steps(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d: None)
    args = make_show_args()
    with pytest.raises(SystemExit):
        cli.cmd_show(args)

def test_cmd_show_calls_imshow(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d: [0, 1, 2])
    monkeypatch.setattr(cli, "load_global", lambda *a, **k: [[1]])
    called = {}
    monkeypatch.setattr(cli, "imshow_field", lambda *a, **k: called.update({"called": True}))
    cli.cmd_show(make_show_args())
    assert called.get("called")

def test_cmd_show_with_meta(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d: [0])
    monkeypatch.setattr(cli, "load_global", lambda *a, **k: [[1]])
    monkeypatch.setattr(cli, "load_metadata", lambda d: {"description": "test"})
    called = {}
    monkeypatch.setattr(cli, "imshow_field", lambda *a, **k: called.update({"called": True, "meta": k.get("metadata")}))
    args = make_show_args(show_meta=True)
    cli.cmd_show(args)
    assert called.get("called")
    assert called.get("meta") == {"description": "test"}

def test_cmd_compare_calls_compare_fields(monkeypatch):
    monkeypatch.setattr(cli, "load_global", lambda *a, **k: [[1]])
    called = {}
    monkeypatch.setattr(cli, "compare_fields", lambda *a, **k: called.update({"called": True}))
    cli.cmd_compare(make_compare_args())
    assert called.get("called")

def test_cmd_compare_with_meta(monkeypatch):
    monkeypatch.setattr(cli, "load_global", lambda *a, **k: [[1]])
    monkeypatch.setattr(cli, "load_metadata", lambda d: {"description": "meta"})
    called = {}
    monkeypatch.setattr(cli, "compare_fields", lambda *a, **k: called.update({"called": True, "meta_a": k.get("metadata_a"), "meta_b": k.get("metadata_b")}))
    args = make_compare_args(show_meta=True)
    cli.cmd_compare(args)
    assert called.get("called")
    assert called.get("meta_a") == {"description": "meta"}
    assert called.get("meta_b") == {"description": "meta"}

def test_cmd_animate_no_steps(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d: None)
    args = make_animate_args()
    with pytest.raises(SystemExit):
        cli.cmd_animate(args)

def test_cmd_animate_with_steps(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d: [0, 1, 2, 3])
    monkeypatch.setattr(cli, "animate_from_outputs", lambda *a, **k: "anim")
    args = make_animate_args(steps="1-2")
    cli.cmd_animate(args)

def test_cmd_animate_with_range(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d: [0, 1, 2, 3, 4])
    monkeypatch.setattr(cli, "animate_from_outputs", lambda *a, **k: "anim")
    args = make_animate_args(start=1, end=3, stride=1)
    cli.cmd_animate(args)

def test_cmd_animate_with_meta(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d: [0])
    monkeypatch.setattr(cli, "animate_from_outputs", lambda *a, **k: "anim")
    monkeypatch.setattr(cli, "load_metadata", lambda d: {"description": "meta"})
    args = make_animate_args(show_meta=True)
    cli.cmd_animate(args)

def test_main_calls_correct_cmd(monkeypatch):
    argv = ["show", "--dir", "outputs"]
    monkeypatch.setattr(sys, "argv", ["prog"] + argv)
    called = {}
    monkeypatch.setattr(cli, "cmd_show", lambda args: called.update({"called": True}))
    cli.main(None)
    assert called.get("called")

def test_help_message(capsys):
    parser = cli.build_parser()
    with pytest.raises(SystemExit):
        parser.parse_args([])
    help_text = parser.format_help()
    assert "show" in help_text
    assert "compare" in help_text
    assert "animate" in help_text
