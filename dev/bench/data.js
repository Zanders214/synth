window.BENCHMARK_DATA = {
  "lastUpdate": 1782392808412,
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
      },
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
          "id": "3dc87002a72e2071db17e2ca2f3718830eae184d",
          "message": "docs: explain how to read the performance dashboard (#9)",
          "timestamp": "2026-06-25T14:03:21+03:00",
          "tree_id": "31a89021384652a95cc94005f2fb930170ffcce0",
          "url": "https://github.com/Zanders214/synth/commit/3dc87002a72e2071db17e2ca2f3718830eae184d"
        },
        "date": 1782385611929,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "Full graph (16 voices + 10 FX)",
            "value": 569795.735,
            "unit": "ns/block"
          },
          {
            "name": "Full graph DSP load @48k/512",
            "value": 5.342,
            "unit": "%"
          },
          {
            "name": "Voice render (16 voices)",
            "value": 322688.895,
            "unit": "ns/block"
          },
          {
            "name": "FX chain (10 slots)",
            "value": 240328.93,
            "unit": "ns/block"
          }
        ]
      },
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
          "id": "ecb37a292b873f89d69216a111ed5892f06bba00",
          "message": "SonarCloud cleanup: safe mechanical code-smell fixes (#10)",
          "timestamp": "2026-06-25T15:04:49+03:00",
          "tree_id": "30f3729c1725c35d4ef243193fadc25bb9f9bbdf",
          "url": "https://github.com/Zanders214/synth/commit/ecb37a292b873f89d69216a111ed5892f06bba00"
        },
        "date": 1782389294656,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "Full graph (16 voices + 10 FX)",
            "value": 566682.991,
            "unit": "ns/block"
          },
          {
            "name": "Full graph DSP load @48k/512",
            "value": 5.313,
            "unit": "%"
          },
          {
            "name": "Voice render (16 voices)",
            "value": 321749.156,
            "unit": "ns/block"
          },
          {
            "name": "FX chain (10 slots)",
            "value": 239861.395,
            "unit": "ns/block"
          }
        ]
      },
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
          "id": "6f9979de99f38097e14ff6dd497e54ffd5be1be2",
          "message": "perf: wavetable inner-loop hoist + fast-math approximations (#11)",
          "timestamp": "2026-06-25T16:03:25+03:00",
          "tree_id": "12304046753ec82d01d9203919187f8063b59634",
          "url": "https://github.com/Zanders214/synth/commit/6f9979de99f38097e14ff6dd497e54ffd5be1be2"
        },
        "date": 1782392807991,
        "tool": "customSmallerIsBetter",
        "benches": [
          {
            "name": "Full graph (16 voices + 10 FX)",
            "value": 384533.713,
            "unit": "ns/block"
          },
          {
            "name": "Full graph DSP load @48k/512",
            "value": 3.605,
            "unit": "%"
          },
          {
            "name": "Voice render (16 voices)",
            "value": 159144.462,
            "unit": "ns/block"
          },
          {
            "name": "FX chain (10 slots)",
            "value": 225499.876,
            "unit": "ns/block"
          }
        ]
      }
    ]
  }
}