// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>

using namespace std;

int main() {
	int i, count;
	for (i = 1; i <= 1000; i++){
        if ((i / 1000 == 3) || (i % 10 == 3) || ((i / 100) % 10 == 3) || ((i / 10) % 10 == 3))
			count++;
	}
	cout << count;
}
// Закомитьте изменения и отправьте их в свой репозиторий.