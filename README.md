# HatanakaCPP
Версия  0.1
## Описание 
HatanakaCPP - это объектно-ориентированная C++ библиотека для сжатия Rinex файлов алгоритмом Hatanaka. 

Библиотека базируется на исходном коде [RNXCMP (Hatanaka, Y. (С))](http://terras.gsi.go.jp/ja/crx2rnx.html) версии 4.0.7 с минимальными изменениями внесенными для совместимости с RTKLIB.

## Установка 

Перенести "Hatanaka.h" и "Hatanaka.cpp" в проект.

Пример кода для сжатия файла "RINEX.16O":
```C++
  CHatanaka hatanaka;
  HTNKRESULT res = hatanaka.compressFile(".\\RINEX.16O", ".\\RINEX.16D");
  hatanaka.closeFiles();
```

## Лицензия

Распространяется по изначальной [лицензии](http://terras.gsi.go.jp/ja/crx2rnx/LICENSE.txt) без дополнительных требований.
