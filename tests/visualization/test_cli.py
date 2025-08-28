import pytest
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


def test_show_parsing(monkeypatch):
    args = run_cmd(
        monkeypatch,
        ["show", "--dir", "outputs", "--fmt", "csv",
         "--overlay-minmax", "--overlay-rankgrid", "--overlay-rankboxes"],
        cli.cmd_show,
    )
    assert args.dir == "outputs"
    assert args.fmt == "csv"
    assert args.overlay_minmax
    assert args.overlay_rankgrid
    assert args.overlay_rankboxes


def test_compare_parsing(monkeypatch):
    args = run_cmd(
        monkeypatch,
        ["compare", "--dir-a", "a", "--dir-b", "b", "--step", "0",
         "--overlay-minmax", "--overlay-rankboxes"],
        cli.cmd_compare,
    )
    assert args.dir_a == "a"
    assert args.dir_b == "b"
    assert args.step == 0
    assert args.overlay_minmax
    assert args.overlay_rankboxes


def test_animate_parsing(monkeypatch):
    args = run_cmd(
        monkeypatch,
        ["animate", "--dir", "out", "--save", "anim.gif",
         "--fps", "15", "--overlay-rankgrid", "--overlay-rankboxes"],
        cli.cmd_animate,
    )
    assert args.dir == "out"
    assert args.save.endswith(".gif")
    assert args.fps == 15
    assert args.overlay_rankgrid
    assert args.overlay_rankboxes


def test_watch_parsing(monkeypatch):
    args = run_cmd(
        monkeypatch,
        ["watch", "--dir", "outputs", "--fmt", "nc",
         "--interval", "1.0", "--title", "Live"],
        cli.cmd_watch,
    )
    assert args.dir == "outputs"
    assert args.fmt == "nc"
    assert pytest.approx(args.interval) == 1.0
    assert args.title == "Live"


def test_interactive_parsing(monkeypatch):
    args = run_cmd(
        monkeypatch,
        ["interactive", "--dir", "outputs", "--fmt", "csv", "--var", "u"],
        cli.cmd_interactive,
    )
    assert args.dir == "outputs"
    assert args.fmt == "csv"
    assert args.var == "u"


def test_help_message(capsys):
    parser = cli.build_parser()
    with pytest.raises(SystemExit):
        parser.parse_args([])
    out, err = capsys.readouterr()
    assert "show" in parser.format_help()
    assert "compare" in parser.format_help()
    assert "animate" in parser.format_help()
    assert "watch" in parser.format_help()
    assert "interactive" in parser.format_help()
