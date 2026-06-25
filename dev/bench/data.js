window.BENCHMARK_DATA = {
  "lastUpdate": 1782380610855,
  "repoUrl": "https://github.com/Zanders214/synth",
  "entries": {
    "ZandersWave DSP": [
      {
        "commit": {
          "author": {
            "email": "152227414+Zanders214@users.noreply.github.com",
            "name": "Dennis Zanders",
            "username": "Zanders214"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "2e51c14d3151c53f1f46fe8f813aa24f2a764d47",
          "message": "Publish benchmark dashboard from dev as well as main (#8)",
          "timestamp": "2026-06-25T12:40:11+03:00",
          "tree_id": "2e9db240ffa5d159a5c901d64c4994cdcc315724",
          "url": "https://github.com/Zanders214/synth/commit/2e51c14d3151c53f1f46fe8f813aa24f2a764d47"
        },
        "date": 1782380610035,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "Full graph (16 voices + 10 FX)",
            "value": 575587.569,
            "unit": "ns/block"
          },
          {
            "name": "Full graph DSP load @48k/512",
            "value": 5.396,
            "unit": "%"
          },
          {
            "name": "Voice render (16 voices)",
            "value": 329801.512,
            "unit": "ns/block"
          },
          {
            "name": "FX chain (10 slots)",
            "value": 240374.586,
            "unit": "ns/block"
          }
        ]
      }
    ]
  }
}