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
