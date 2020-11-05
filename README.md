## compilation
```bash
mkdir build
cd build
cmake ..
make
```

# min configuration
config.txt обязателен для запуска
```bash
cp ../config.txt .
```

папка со статикой
```bash
mkdir www
```

папка со статикой
```bash
mkdir www
```

создаем там файл со строчкой
```bash
echo "<html><body>hello</body></html>" > www/index.html
```

# так же редактируем url БД в 'config.txt' и запускаем БД перед запуском сервера (а то ругаться будет)


## dependencies
* libpqxx
* postgresql
* stdlib
* nlohmann JSON (static single include)
* static SHA1 lib (да бардак вообще и не говорите)

## TODO
* переписать на С без вышеназванных депенденсиз, оставив только syscalls и char'ы с int'ами
не тудушка, а мор времени - но заняться можно )

По поводу этого сервера могу только сказать что он:
- написан в торопях
- в первый раз, с использованием стандартных сокетов (на уровне чтения и записи в файл)
- без грамотной организации использования оперативной памяти
- и вообще там просто один процесс читает в оперативку (резервируя по 10МБ на соединение) и потом сам же с базой работает и отвечает
- с решениями вызванными нуждой в экономии времени на разработку
- работает все равно миллисекунды:)
- обратил внимание, что разраб epoll продумывая usercase подчеркивал скорее индивидуальность и авторство управления событиями на сокетах, чем универсальность применения и вообще слово usercase не знает, к примеру :)
