import pytest
import visualization.cli as cli
import argparse, csv

def make_csv(path, rows):
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(
            f, fieldnames=["rank", "x_offset", "y_offset", "nx_local", "ny_local"]
        )
        writer.writeheader()
        writer.writerows(rows)

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
        fmt="nc",
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
        overlay_rankgrid=False,
        overlay_rankboxes=False,
    )
    base.update(overrides)
    return argparse.Namespace(**base)

def make_watch_args(**overrides):
    base = dict(
        dir="outputs",
        fmt="csv",
        var="u",
        interval=0.0,
        cmap="viridis",
        vmin=None,
        vmax=None,
        title=None,
        no_colorbar=False,
        tight=False,
        redraw_same=False,
    )
    base.update(overrides)
    return argparse.Namespace(**base)


def make_interactive_args(**overrides):
    base = dict(
        dir="outputs",
        fmt="csv",
        var="u",
        cmap="viridis",
    )
    base.update(overrides)
    import argparse
    return argparse.Namespace(**base)

def test_none_returns_all():
    assert cli._parse_steps_arg(None, [1, 2, 3]) == [1, 2, 3]

def test_single_value():
    assert cli._parse_steps_arg("5", list(range(10))) == [5]

def test_comma_separated():
    assert cli._parse_steps_arg("1,3,5", list(range(10))) == [1, 3, 5]

def test_range_full():
    avail = list(range(10))
    assert cli._parse_steps_arg("2-5", avail) == [2, 3, 4, 5]

def test_range_open_start_and_end():
    avail = list(range(5, 15))
    assert cli._parse_steps_arg("-8", avail) == [5, 6, 7, 8]
    assert cli._parse_steps_arg("12-", avail) == [12, 13, 14]

def test_empty_string():
    assert cli._parse_steps_arg("", [1, 2, 3]) == []


def test_no_file_returns_none(tmp_path):
    assert cli.load_rank_layout(tmp_path) is None

def test_load_rank_layout(tmp_path):
    rows = [
        {"rank": "0", "x_offset": "0", "y_offset": "0", "nx_local": "128", "ny_local": "128"},
        {"rank": "1", "x_offset": "128", "y_offset": "0", "nx_local": "128", "ny_local": "128"},
    ]
    csv_file = tmp_path / "rank_layout.csv"
    make_csv(csv_file, rows)

    ranks = cli.load_rank_layout(tmp_path)
    assert ranks == [
        (0, 0, 0, 128, 128),
        (1, 128, 0, 128, 128),
    ]

def test_cmd_show_executes(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d, fmt: [0, 1, 2])
    monkeypatch.setattr(cli, "load_global", lambda d, step, fmt, var: [[1, 2], [3, 4]])

    called = {}
    def fake_imshow_field(U, **kwargs):
        called.update({"U": U, "kwargs": kwargs})
        return "fig", "ax"
    monkeypatch.setattr(cli, "imshow_field", fake_imshow_field)

    monkeypatch.setattr(cli, "load_rank_layout", lambda d: [(0, 0, 0, 128, 128)])

    args = argparse.Namespace(
        dir="outputs",
        fmt="csv",
        step=None,
        var="u",
        title=None,
        cmap="viridis",
        vmin=None,
        vmax=None,
        no_colorbar=False,
        show=False,
        save=None,
        overlay_minmax=True,
        overlay_rankgrid=True,
        overlay_rankboxes=True,
    )

    cli.cmd_show(args)

    assert called["U"] == [[1, 2], [3, 4]]
    assert called["kwargs"]["overlay_minmax"] is True
    assert called["kwargs"]["overlay_rankgrid"] is True
    assert called["kwargs"]["rank_layout"] == [(0, 0, 0, 128, 128)]

def test_cmd_show_no_steps(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d, fmt: [])

    args = argparse.Namespace(
        dir="outputs",
        fmt="csv",
        step=None,
        var="u",
        title=None,
        cmap="viridis",
        vmin=None,
        vmax=None,
        no_colorbar=False,
        show=False,
        save=None,
        overlay_minmax=False,
        overlay_rankgrid=False,
        overlay_rankboxes=False,
    )

    with pytest.raises(SystemExit) as excinfo:
        cli.cmd_show(args)

    assert "No snapshots found" in str(excinfo.value)


def test_cmd_compare_invokes(monkeypatch):
    called = {}

    def fake_load_global(path, step, fmt, var):
        return f"data-{path}-{step}-{fmt}-{var}"
    monkeypatch.setattr(cli, "load_global", fake_load_global)

    def fake_compare_fields(A, B, **kwargs):
        called["A"] = A
        called["B"] = B
        called["kwargs"] = kwargs
    monkeypatch.setattr(cli, "compare_fields", fake_compare_fields)

    args = argparse.Namespace(
        dir_a="a",
        dir_b="b",
        fmt_a="csv",
        fmt_b="nc",
        var_a="u",
        var_b="v",
        step=42,
        title_a="Left",
        title_b="Right",
        cmap="viridis",
        vmin=None,
        vmax=None,
        split_colorbar=False,
        no_diff=False,
        diff_cmap="coolwarm",
        diff_vlim=None,
        show=False,
        save=None,
        overlay_minmax=True,
        overlay_rankboxes=True,
    )

    cli.cmd_compare(args)

    assert called["A"].startswith("data-a-42-csv-u")
    assert called["B"].startswith("data-b-42-nc-v")
    assert called["kwargs"]["overlay_minmax"] is True
    assert called["kwargs"]["overlay_rankboxes"] is True


def test_cmd_animate_no_steps(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d, fmt: [])
    args = make_animate_args()
    with pytest.raises(SystemExit):
        cli.cmd_animate(args)

def test_cmd_animate_default(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d, fmt: [0, 1, 2, 3])
    monkeypatch.setattr(cli, "load_rank_layout", lambda d: None)

    captured = {}
    monkeypatch.setattr(cli, "animate_from_outputs", lambda *a, **kw: captured.update(kw))

    args = make_animate_args()
    cli.cmd_animate(args)

    assert captured["steps"] == [0, 1, 2, 3]
    assert captured["fps"] == 10
    assert captured["writer"] == "pillow"

def test_cmd_animate_range(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d, fmt: list(range(10)))
    monkeypatch.setattr(cli, "load_rank_layout", lambda d: None)

    captured = {}
    monkeypatch.setattr(cli, "animate_from_outputs", lambda *a, **kw: captured.update(kw))

    args = make_animate_args(start=2, end=6, stride=2)
    cli.cmd_animate(args)
    assert captured["steps"] == [2, 4, 6]

def test_cmd_animate_explicit_steps(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d, fmt: list(range(10)))
    monkeypatch.setattr(cli, "load_rank_layout", lambda d: None)

    captured = {}
    monkeypatch.setattr(cli, "animate_from_outputs", lambda *a, **kw: captured.update(kw))

    args = make_animate_args(steps="1,3,5")
    cli.cmd_animate(args)
    assert captured["steps"] == [1, 3, 5]

def test_cmd_animate_with_rankgrid(monkeypatch):
    monkeypatch.setattr(cli, "list_available_steps", lambda d, fmt: [0, 1])
    monkeypatch.setattr(cli, "load_rank_layout", lambda d: [("rank0", 0, 0, 10, 10)])

    captured = {}
    monkeypatch.setattr(cli, "animate_from_outputs", lambda *a, **kw: captured.update(kw))
    
    args = make_animate_args(overlay_rankgrid=True)
    cli.cmd_animate(args)
    assert captured["rank_layout"] == [("rank0", 0, 0, 10, 10)]
    assert captured["overlay_rankgrid"] is True


def test_cmd_watch_single_iteration(monkeypatch):
    monkeypatch.setattr(cli.time, "sleep", lambda s: None)

    monkeypatch.setattr(cli.io, "list_available_steps", lambda *a, **k: [1])
    monkeypatch.setattr(cli.io, "load_global", lambda *a, **k: [[1, 2], [3, 4]])

    class FakeIm:
        def set_data(self, U): self.data = U
        def set_clim(self, **kw): self.clim = kw
    class FakeAx:
        def __init__(self): self.title = None
        def imshow(self, *a, **k): return FakeIm()
        def set_title(self, t): self.title = t
    class FakeFig:
        def __init__(self): self.canvas = self
        def colorbar(self, *a, **k): return "cbar"
        def tight_layout(self): pass
        def draw(self): pass
        def flush_events(self): pass
    fake_ax = FakeAx()
    fake_fig = FakeFig()

    monkeypatch.setattr(cli.plt, "ion", lambda: None)
    monkeypatch.setattr(cli.plt, "subplots", lambda: (fake_fig, fake_ax))

    def fake_draw():
        raise KeyboardInterrupt()
    fake_fig.draw = fake_draw

    args = make_watch_args()
    cli.cmd_watch(args)

    assert fake_ax.title.startswith("Step")

def test_cmd_watch_file_not_found(monkeypatch):
    monkeypatch.setattr(cli.time, "sleep", lambda s: None)

    calls = {"n": 0}
    def fake_list_available_steps(*a, **k):
        calls["n"] += 1
        if calls["n"] == 1:
            raise FileNotFoundError()
        else:
            raise KeyboardInterrupt()
    monkeypatch.setattr(cli.io, "list_available_steps", fake_list_available_steps)

    monkeypatch.setattr(cli.io, "load_global", lambda *a, **k: [[1]])
    monkeypatch.setattr(cli.plt, "ion", lambda: None)
    monkeypatch.setattr(cli.plt, "subplots", lambda: ("fig", "ax"))

    args = make_watch_args()
    cli.cmd_watch(args)

    assert calls["n"] == 2

def test_cmd_watch_with_colorbar(monkeypatch):
    monkeypatch.setattr(cli.time, "sleep", lambda s: None)
    monkeypatch.setattr(cli.io, "list_available_steps", lambda *a, **k: [1])
    monkeypatch.setattr(cli.io, "load_global", lambda *a, **k: [[1, 2], [3, 4]])

    called = {}

    class FakeIm:
        def set_data(self, U): pass
        def set_clim(self, **kw): pass
    class FakeAx:
        def set_title(self, t): called["title"] = t
        def imshow(self, *a, **k): return FakeIm()
    class FakeFig:
        def __init__(self): self.canvas = self
        def colorbar(self, *a, **k): called["colorbar"] = True
        def tight_layout(self): called["tight"] = True
        def draw(self): raise KeyboardInterrupt()
        def flush_events(self): pass

    monkeypatch.setattr(cli.plt, "ion", lambda: None)
    monkeypatch.setattr(cli.plt, "subplots", lambda: (FakeFig(), FakeAx()))

    args = make_watch_args(no_colorbar=False, tight=True)
    cli.cmd_watch(args)

    assert called.get("colorbar") is True
    assert "title" in called
    assert "tight" in called

def test_cmd_watch_no_steps(monkeypatch):
    monkeypatch.setattr(cli.time, "sleep", lambda s: None)

    calls = {"n": 0}
    def fake_list_available_steps(*a, **k):
        calls["n"] += 1
        if calls["n"] == 1:
            return []
        raise KeyboardInterrupt()
    monkeypatch.setattr(cli.io, "list_available_steps", fake_list_available_steps)
    monkeypatch.setattr(cli.io, "load_global", lambda *a, **k: [[1]])
    monkeypatch.setattr(cli.plt, "ion", lambda: None)
    monkeypatch.setattr(cli.plt, "subplots", lambda: ("fig", "ax"))

    args = make_watch_args()
    cli.cmd_watch(args)
    assert calls["n"] == 2

def test_cmd_watch_update_branch(monkeypatch):
    monkeypatch.setattr(cli.time, "sleep", lambda s: None)

    steps = [[1], [2]]
    def fake_list_available_steps(*a, **k):
        if steps:
            return steps.pop(0)
        raise KeyboardInterrupt()
    monkeypatch.setattr(cli.io, "list_available_steps", fake_list_available_steps)
    monkeypatch.setattr(cli.io, "load_global", lambda *a, **k: [[1, 2]])

    called = {}

    class FakeIm:
        def set_data(self, U): called["set_data"] = True
        def set_clim(self, **kw): called["set_clim"] = True
    class FakeAx:
        def imshow(self, *a, **k): return FakeIm()
        def set_title(self, t): called["set_title"] = True
    class FakeFig:
        def __init__(self): self.canvas = self
        def colorbar(self, *a, **k): pass
        def draw(self): pass
        def flush_events(self): pass
        def tight_layout(self): called["tight"] = True

    monkeypatch.setattr(cli.plt, "ion", lambda: None)
    monkeypatch.setattr(cli.plt, "subplots", lambda: (FakeFig(), FakeAx()))

    args = make_watch_args(no_colorbar=True, tight=True)
    args.vmin = None
    args.vmax = None
    cli.cmd_watch(args)

    assert called["set_data"]
    assert called["set_title"]
    assert called["tight"]

def test_cmd_watch_redraw_same(monkeypatch):
    monkeypatch.setattr(cli.time, "sleep", lambda s: None)

    steps = [[1], [1]]
    def fake_list_available_steps(*a, **k):
        if steps:
            return steps.pop(0)
        raise KeyboardInterrupt()
    monkeypatch.setattr(cli.io, "list_available_steps", fake_list_available_steps)
    monkeypatch.setattr(cli.io, "load_global", lambda *a, **k: [[1]])

    class FakeIm:
        def set_data(self, U): pass
        def set_clim(self, **kw): pass

    class FakeAx:
        def imshow(self, *a, **k): return FakeIm()
        def set_title(self, t): pass

    class FakeFig:
        def __init__(self): self.canvas = self
        def colorbar(self, *a, **k): pass
        def draw(self): pass
        def flush_events(self): pass

    monkeypatch.setattr(cli.plt, "ion", lambda: None)
    monkeypatch.setattr(cli.plt, "subplots", lambda: (FakeFig(), FakeAx()))

    args = make_watch_args(redraw_same=False)
    cli.cmd_watch(args)

def test_cmd_interactive_no_steps(monkeypatch):
    monkeypatch.setattr(cli.io, "list_available_steps", lambda *a, **k: [])
    args = make_interactive_args()
    with pytest.raises(SystemExit):
        cli.cmd_interactive(args)

def test_cmd_interactive_key_navigation(monkeypatch):
    steps = [0, 1, 2]
    monkeypatch.setattr(cli.io, "list_available_steps", lambda *a, **k: steps)
    monkeypatch.setattr(cli.io, "load_global", lambda *a, **k: [[1]])

    called = {}

    class FakeIm:
        def set_data(self, U): called["set_data"] = True
    class FakeTtl:
        def set_text(self, t): called["set_text"] = t
    class FakeAx:
        def imshow(self, U, **kw): return FakeIm()
        def set_title(self, t): return FakeTtl()
    class FakeFig:
        def __init__(self): self.canvas = self
        def draw_idle(self): called["draw_idle"] = True
        def mpl_connect(self, evt, cb):
            # simulate pressing right then left
            cb(type("E", (), {"key":"right"}))
            cb(type("E", (), {"key":"left"}))
    monkeypatch.setattr(cli.plt, "subplots", lambda: (FakeFig(), FakeAx()))
    monkeypatch.setattr(cli.plt, "show", lambda: None)

    args = make_interactive_args()
    cli.cmd_interactive(args)

    assert called["set_data"]
    assert "set_text" in called
    assert "draw_idle" in called

def test_help_message(capsys):
    parser = cli.build_parser()
    with pytest.raises(SystemExit):
        parser.parse_args([])
    capsys.readouterr()
    assert "show" in parser.format_help()
    assert "compare" in parser.format_help()
    assert "animate" in parser.format_help()
    assert "watch" in parser.format_help()
    assert "interactive" in parser.format_help()
