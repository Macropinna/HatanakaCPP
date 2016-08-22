# HatanakaCPP
Версия  0.1
## Описание 
HatanakaCPP - это объектно-ориентированная C++ библиотека для сжатия Rinex файлов алгоритмом Hatanaka. 

Библиотека базируется на исходном коде RNXCMP (Hatanaka, Y. (С)) версии 4.0.7.

## Установка 

Перенести "hatanaka.h" и "hatanaka.cpp" в проект.

Пример кода для сжатие файла "RINEX.16O":
```C++
  CHatanaka hatanaka;
  HTNKRESULT res = hatanaka.compressFile(".\\RINEX.16O", ".\\RINEX.16D");
  hatanaka.closeFiles();
```

## Лицензия

Распространяется по изначальной [лицензии](http://terras.gsi.go.jp/ja/crx2rnx/LICENSE.txt) без дополнительных требований.
