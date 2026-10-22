#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "server/registro.h"
#include <doctest/doctest.h>

using namespace std::chrono_literals;
using ventrik::servidor::Registro;

TEST_CASE("El registro escribe todas las líneas, en orden") {
  std::mutex candado;
  std::vector<std::string> escritas;
  {
    Registro registro(
        [&](const std::string& linea) {
          const std::lock_guard<std::mutex> cerrado(candado);
          escritas.push_back(linea);
        },
        "perdidas:");
    for (int i = 0; i < 100; ++i) {
      registro.agregar("linea " + std::to_string(i));
    }
  }
  REQUIRE(escritas.size() == 100);
  CHECK(escritas.front() == "linea 0");
  CHECK(escritas.back() == "linea 99");
}

TEST_CASE("Con la consola trabada, el registro no frena a quien avisa y después dice cuántas perdió") {
  std::atomic<bool> escribiendo{false};
  std::atomic<bool> destrabar{false};
  std::mutex candado;
  std::vector<std::string> escritas;
  {
    Registro registro(
        [&](const std::string& linea) {
          escribiendo = true;
          while (!destrabar.load()) {
            std::this_thread::sleep_for(1ms);
          }
          const std::lock_guard<std::mutex> cerrado(candado);
          escritas.push_back(linea);
        },
        "perdidas:", 4);
    registro.agregar("primera");
    while (!escribiendo.load()) {
      std::this_thread::sleep_for(1ms);
    }
    const auto antes = std::chrono::steady_clock::now();
    for (int i = 0; i < 10; ++i) {
      registro.agregar("otra " + std::to_string(i));
    }
    CHECK(std::chrono::steady_clock::now() - antes < 1s);
    destrabar = true;
  }
  REQUIRE(escritas.size() == 6);
  CHECK(escritas[0] == "primera");
  CHECK(escritas[1] == "perdidas: 6");
  CHECK(escritas[2] == "otra 0");
  CHECK(escritas[5] == "otra 3");
}
