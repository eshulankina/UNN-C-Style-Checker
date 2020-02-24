## Telegram chat

https://t.me/unncompilercourse  
![qr-telega](https://user-images.githubusercontent.com/22346834/75149496-339ace80-5713-11ea-8030-a67df1ca1b81.png)

## Установка окружения
### Windows 10
Вам потребуется установить `WSL`. Для этого нужно следовать   [инструкции](https://docs.microsoft.com/ru-ru/windows/wsl/install-win10).      
Обратите внимание что нужно выбрать версию `Ubuntu18`     
После установки запускаем `Ubuntu 18.04 LTS` у вас должен появиться терминал 
### Ubuntu18
Пользователям linux можно приступать к следующему шагу

## Сборка LLVM & CLang
Находясь в терминале установите следующие зависимости:
```
sudo apt install git cmake 
```

Создайте папку для работы с курсом и перейдите в нее
```
mkdir ~/compiler-course && cd ~/compiler-course
```

Далее собираем `LLVM` вместе с `Clang` из исходников
```
git clone https://github.com/llvm/llvm-project
cd llvm-project/llvm/
mkdir build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=install \
          -DLLVM_TARGETS_TO_BUILD=X86 \
          -DLLVM_ENABLE_PROJECTS=clang
make -j4
make install
```
Сборка может занять длительное время

### Задача
После успешного построения `LLVM & Clang` можно приступать к выполнению задания.

Вернитесь в корневую папку курса:
```
cd ~/compiler-course
```

Для начала сделайте [Fork](http://gearmobile.github.io/git/fork-github/) текущего репозитория к себе в профиль. После этого копия репозитория появится у вас в профиле.

Склонируйте и перейдите в локальную копию:
```
git clone <fork-repo-url>
cd UNN-C-Style-Checker/
```
Создайте отдельную ветку для работы с репозиторием, которая будет содержать имя, фамилию и направление, пример:
```
git checkout -b vasya-pupkin-fiit
```
Собираем проект с заданием
```
mkdir build && cd build
cmake ../ -DLLVM_DIR=~/compiler-course/workspace/llvm-project/llvm/build/install/lib/cmake
make -j8
```

Запускаем
```
./c-style-checker ../test/test.cpp
```

Вам необходимо дописать свой код в `main.cpp` чтобы утилита смогла заменить все преобразования типов в стиле си на соответствующий им аналог из с++

После того как вы сделали заданиe:
1. Сделайте коммит:
    ```
    git add -u
    git commit -m 'Complete c-style-checker task'
    ```
2. Отправьте ветку с вашим решением на сервер:
    ```
    git push origin HEAD
    ```
3. Перейдите на github и откройте `pull request` из вашей ветки в `master` главного репозитория

Если необходимо что то исправить после создания `pull request` нужно просто сделать изменения и повторить шаги 1 и 2
Не нужно открывать новый `pull request`
