Etagere:

Build the dynamic libraries locally by:
- Edit `Cargo.toml` and add this section:
  ```
  [lib]
  crate-type = ["cdylib"]
  ```
- Build the dynamic library with `cargo build --release --features "ffi"`, it will be under `target/release`.
- Generate header file with `cbindgen --config cbindgen.toml --crate etagere --output etagere.h`. You might need to do
  `cargo install cbindgen` first.

Mac M1 Max (Xcode / clang 15):
```txt
Run smol-atlas unit tests...
Run   mapbox on synthetic: 1988 (+32000/-30012): atlas 4096x3945 (16.2Mpix) used 8.2Mpix (51.0%), 75.9ms
Run  etagere on synthetic: 1988 (+32000/-30012): atlas 4096x4096 (16.8Mpix) used 8.2Mpix (49.1%), 7.8ms
Run     smol on synthetic: 1988 (+32000/-30012): atlas 4096x3515 (14.4Mpix) used 8.2Mpix (57.2%), 17.6ms
Test data 'test/thumbs-gold.txt': 252 frames; 4692 unique 15839 total entries
Run   mapbox on data file: 2789 (+145600/-142811 30 runs): atlas 4096x4087 (16.7Mpix) used 7.5Mpix (44.8%), 346.8ms
Run  etagere on data file: 2789 (+145600/-142811 30 runs): atlas 4096x4096 (16.8Mpix) used 7.5Mpix (44.7%), 31.3ms
Run     smol on data file: 2789 (+145600/-142811 30 runs): atlas 4096x2170 (8.9Mpix) used 7.5Mpix (84.4%), 39.9ms
Test data 'test/thumbs-wingit.txt': 198 frames; 2563 unique 14286 total entries
Run   mapbox on data file: 334 (+77643/-77309 30 runs): atlas 8192x4032 (33.0Mpix) used 4.4Mpix (13.2%), 90.5ms
Run  etagere on data file: 334 (+77643/-77309 30 runs): atlas 4096x4096 (16.8Mpix) used 4.4Mpix (26.0%), 19.9ms
Run     smol on data file: 334 (+77643/-77309 30 runs): atlas 4095x3456 (14.2Mpix) used 4.4Mpix (30.9%), 23.8ms
Test data 'test/thumbs-sprite-fright.txt': 27 frames; 5108 unique 7213 total entries
Run   mapbox on data file: 949 (+156960/-156011 30 runs): atlas 4096x3959 (16.2Mpix) used 3.5Mpix (21.8%), 206.8ms
Run  etagere on data file: 949 (+156960/-156011 30 runs): atlas 4096x4096 (16.8Mpix) used 3.5Mpix (21.1%), 18.7ms
Run     smol on data file: 949 (+156960/-156011 30 runs): atlas 4096x2889 (11.8Mpix) used 3.5Mpix (29.9%), 27.7ms
```

Windows Ryzen 5950X (VS 2022):
```txt
Run smol-atlas unit tests...
Run   mapbox on synthetic: 1988 (+32000/-30012): atlas 4096x3758 (15.4Mpix) used 8.3Mpix (53.9%), 61.0ms
Run  etagere on synthetic: 1988 (+32000/-30012): atlas 4096x4096 (16.8Mpix) used 8.3Mpix (49.4%), 7.0ms
Run     smol on synthetic: 1988 (+32000/-30012): atlas 4096x3464 (14.2Mpix) used 8.3Mpix (58.4%), 24.0ms
Test data 'test/thumbs-gold.txt': 252 frames; 4692 unique 15839 total entries
Run   mapbox on data file: 2789 (+145600/-142811 30 runs): atlas 4096x4087 (16.7Mpix) used 7.5Mpix (44.8%), 336.0ms
Run  etagere on data file: 2789 (+145600/-142811 30 runs): atlas 4096x4096 (16.8Mpix) used 7.5Mpix (44.7%), 60.0ms
Run     smol on data file: 2789 (+145600/-142811 30 runs): atlas 4096x2170 (8.9Mpix) used 7.5Mpix (84.4%), 75.0ms
Test data 'test/thumbs-wingit.txt': 198 frames; 2563 unique 14286 total entries
Run   mapbox on data file: 334 (+77643/-77309 30 runs): atlas 8192x4032 (33.0Mpix) used 4.4Mpix (13.2%), 97.0ms
Run  etagere on data file: 334 (+77643/-77309 30 runs): atlas 4096x4096 (16.8Mpix) used 4.4Mpix (26.0%), 35.0ms
Run     smol on data file: 334 (+77643/-77309 30 runs): atlas 4095x3456 (14.2Mpix) used 4.4Mpix (30.9%), 43.0ms
Test data 'test/thumbs-sprite-fright.txt': 27 frames; 5108 unique 7213 total entries
Run   mapbox on data file: 949 (+156960/-156011 30 runs): atlas 4096x3959 (16.2Mpix) used 3.5Mpix (21.8%), 197.0ms
Run  etagere on data file: 949 (+156960/-156011 30 runs): atlas 4096x4096 (16.8Mpix) used 3.5Mpix (21.1%), 32.0ms
Run     smol on data file: 949 (+156960/-156011 30 runs): atlas 4096x2889 (11.8Mpix) used 3.5Mpix (29.9%), 47.0ms
```

Manually growable etagere and mapbox, plus stb_rectpack, plus rectallocator:
```txt
Run smol-atlas unit tests...
Run   mapbox on synthetic: 1988 (+32000/-30012): atlas 4096x4096 (16.8Mpix) used 8.2Mpix (48.8%), 64.0ms
Run  etagere on synthetic: 1988 (+32000/-30012): atlas 3584x3584 (12.8Mpix) used 8.2Mpix (64.2%), 8.0ms
Run rectpack on synthetic: 1988 (+32000/-30012): atlas 6144x5632 (34.6Mpix) used 8.1Mpix (23.4%), 37.0ms
Run awralloc on synthetic: 1988 (+32000/-30012): atlas 3584x3584 (12.8Mpix) used 8.3Mpix (64.3%), 161.0ms
Run     smol on synthetic: 1988 (+32000/-30012): atlas 4096x4096 (16.8Mpix) used 8.3Mpix (49.4%), 24.0ms
Test data 'test/thumbs-gold.txt': 252 frames; 4692 unique 15839 total entries
Run   mapbox on data file: 2789 (+145600/-142811 30 runs): atlas 4608x4096 (18.9Mpix) used 7.5Mpix (39.8%), 316.0ms
Run  etagere on data file: 2789 (+145600/-142811 30 runs): atlas 3584x3072 (11.0Mpix) used 7.5Mpix (68.2%), 29.0ms
Run rectpack on data file: 2789 (+145600/-142811 30 runs): atlas 8704x8704 (75.8Mpix) used 7.5Mpix (9.9%), 268.0ms
Run awralloc on data file: 2789 (+145600/-142811 30 runs): atlas 3584x3072 (11.0Mpix) used 7.5Mpix (68.2%), 576.0ms
Run     smol on data file: 2789 (+145600/-142811 30 runs): atlas 4096x4096 (16.8Mpix) used 7.5Mpix (44.7%), 40.0ms
Test data 'test/thumbs-wingit.txt': 198 frames; 2563 unique 14286 total entries
Run   mapbox on data file: 334 (+77643/-77309 30 runs): atlas 5120x4608 (23.6Mpix) used 4.4Mpix (18.5%), 77.0ms
Run  etagere on data file: 334 (+77643/-77309 30 runs): atlas 4096x4096 (16.8Mpix) used 4.4Mpix (26.0%), 20.0ms
Run rectpack on data file: 334 (+77643/-77309 30 runs): atlas 9728x9728 (94.6Mpix) used 4.4Mpix (4.6%), 71.0ms
Run awralloc on data file: 334 (+77643/-77309 30 runs): atlas 4608x4096 (18.9Mpix) used 4.4Mpix (23.1%), 149.0ms
Run     smol on data file: 334 (+77643/-77309 30 runs): atlas 4096x4096 (16.8Mpix) used 4.4Mpix (26.0%), 24.0ms
Test data 'test/thumbs-sprite-fright.txt': 27 frames; 5108 unique 7213 total entries
Run   mapbox on data file: 949 (+156960/-156011 30 runs): atlas 4096x4096 (16.8Mpix) used 3.5Mpix (21.1%), 214.0ms
Run  etagere on data file: 949 (+156960/-156011 30 runs): atlas 4096x3584 (14.7Mpix) used 3.5Mpix (24.1%), 20.0ms
Run rectpack on data file: 949 (+156960/-156011 30 runs): atlas 9216x8704 (80.2Mpix) used 3.5Mpix (4.4%), 324.0ms
Run awralloc on data file: 949 (+156960/-156011 30 runs): atlas 4096x3584 (14.7Mpix) used 3.5Mpix (24.1%), 816.0ms
Run     smol on data file: 949 (+156960/-156011 30 runs): atlas 4096x4096 (16.8Mpix) used 3.5Mpix (21.1%), 31.0ms
```

Manually growable smol_atlas too:
```txt
Run smol-atlas unit tests...
Run   mapbox on synthetic: 1988 (+32000/-30012): atlas 4096x4096 (16.8Mpix) used 8.2Mpix (48.8%), 64.0ms
Run  etagere on synthetic: 1988 (+32000/-30012): atlas 3584x3584 (12.8Mpix) used 8.2Mpix (64.2%), 8.0ms
Run rectpack on synthetic: 1988 (+32000/-30012): atlas 6144x5632 (34.6Mpix) used 8.1Mpix (23.4%), 37.0ms
Run awralloc on synthetic: 1988 (+32000/-30012): atlas 3584x3584 (12.8Mpix) used 8.3Mpix (64.3%), 161.0ms
Run     smol on synthetic: 1988 (+32000/-30012): atlas 4096x4096 (16.8Mpix) used 8.2Mpix (48.8%), 28.0ms
Test data 'test/thumbs-gold.txt': 252 frames; 4692 unique 15839 total entries
Run   mapbox on data file: 2789 (+145600/-142811 30 runs): atlas 4608x4096 (18.9Mpix) used 7.5Mpix (39.8%), 315.0ms
Run  etagere on data file: 2789 (+145600/-142811 30 runs): atlas 3584x3072 (11.0Mpix) used 7.5Mpix (68.2%), 31.0ms
Run rectpack on data file: 2789 (+145600/-142811 30 runs): atlas 8704x8704 (75.8Mpix) used 7.5Mpix (9.9%), 267.0ms
Run awralloc on data file: 2789 (+145600/-142811 30 runs): atlas 3584x3072 (11.0Mpix) used 7.5Mpix (68.2%), 583.0ms
Run     smol on data file: 2789 (+145600/-142811 30 runs): atlas 3584x3072 (11.0Mpix) used 7.5Mpix (68.2%), 40.0ms
Test data 'test/thumbs-wingit.txt': 198 frames; 2563 unique 14286 total entries
Run   mapbox on data file: 334 (+77643/-77309 30 runs): atlas 5120x4608 (23.6Mpix) used 4.4Mpix (18.5%), 78.0ms
Run  etagere on data file: 334 (+77643/-77309 30 runs): atlas 4096x4096 (16.8Mpix) used 4.4Mpix (26.0%), 19.0ms
Run rectpack on data file: 334 (+77643/-77309 30 runs): atlas 9728x9728 (94.6Mpix) used 4.4Mpix (4.6%), 72.0ms
Run awralloc on data file: 334 (+77643/-77309 30 runs): atlas 4608x4096 (18.9Mpix) used 4.4Mpix (23.1%), 144.0ms
Run     smol on data file: 334 (+77643/-77309 30 runs): atlas 4096x3584 (14.7Mpix) used 4.4Mpix (29.8%), 26.0ms
Test data 'test/thumbs-sprite-fright.txt': 27 frames; 5108 unique 7213 total entries
Run   mapbox on data file: 949 (+156960/-156011 30 runs): atlas 4096x4096 (16.8Mpix) used 3.5Mpix (21.1%), 214.0ms
Run  etagere on data file: 949 (+156960/-156011 30 runs): atlas 4096x3584 (14.7Mpix) used 3.5Mpix (24.1%), 22.0ms
Run rectpack on data file: 949 (+156960/-156011 30 runs): atlas 9216x8704 (80.2Mpix) used 3.5Mpix (4.4%), 323.0ms
Run awralloc on data file: 949 (+156960/-156011 30 runs): atlas 4096x3584 (14.7Mpix) used 3.5Mpix (24.1%), 809.0ms
Run     smol on data file: 949 (+156960/-156011 30 runs): atlas 3584x3584 (12.8Mpix) used 3.5Mpix (27.6%), 34.0ms
```
