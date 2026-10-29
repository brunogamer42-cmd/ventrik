#include "server/comprobador.h"

#include <algorithm>
#include <string_view>
#include <system_error>
#include <utility>

#include <windows.h>

namespace ventrik::servidor {

Comprobador::Comprobador(const red::Despertador& despertador, std::size_t maximo)
    : despertador_(despertador), maximo_(maximo) {}

Comprobador::~Comprobador() {
  {
    const std::lock_guard<std::mutex> cerrado(candado_);
    terminar_ = true;
  }
  hay_pedidos_.notify_all();
  if (hilo_.joinable()) {
    hilo_.join();
  }
}

bool Comprobador::arrancar() {
  if (!hilo_.joinable()) {
    try {
      hilo_ = std::thread([this] { trabajar(); });
    } catch (const std::system_error&) {
      return false;
    }
    SetThreadPriority(hilo_.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
  }
  return true;
}

bool Comprobador::pedir(const Comprobacion& quien, const ContrasenaGuardada& guardada,
                        cifrado::Secreto contrasena) {
  {
    const std::unique_lock<std::mutex> cerrado(candado_, std::try_to_lock);
    if (!cerrado || !hilo_.joinable() || pedidos_.size() + (en_curso_ ? 1 : 0) >= maximo_ ||
        en_curso_ == quien.ip ||
        std::any_of(pedidos_.begin(), pedidos_.end(),
                    [&quien](const Pedido& pedido) { return pedido.quien.ip == quien.ip; })) {
      return false;
    }
    pedidos_.push_back(Pedido{quien, guardada, std::move(contrasena)});
  }
  hay_pedidos_.notify_one();
  return true;
}

std::vector<Comprobacion> Comprobador::tomar() {
  if (!hay_hechas_.load()) {
    return {};
  }
  const std::unique_lock<std::mutex> cerrado(candado_, std::try_to_lock);
  if (!cerrado) {
    return {};
  }
  hay_hechas_ = false;
  return std::exchange(hechas_, {});
}

void Comprobador::trabajar() {
  std::unique_lock<std::mutex> cerrado(candado_);
  for (;;) {
    hay_pedidos_.wait(cerrado, [this] { return terminar_ || !pedidos_.empty(); });
    if (terminar_) {
      return;
    }
    Pedido pedido = std::move(pedidos_.front());
    pedidos_.pop_front();
    en_curso_ = pedido.quien.ip;
    cerrado.unlock();
    const std::string_view contrasena(reinterpret_cast<const char*>(pedido.contrasena.bytes().data()),
                                      pedido.contrasena.size());
    pedido.quien.resultado = pedido.quien.cambio ? Comprobado::coincide : comprobar(pedido.guardada, contrasena);
    if (pedido.quien.resultado == Comprobado::coincide && (pedido.quien.cambio || pedido.quien.rehacer)) {
      pedido.quien.hash = guardar_contrasena(contrasena);
    }
    pedido.contrasena = cifrado::Secreto();
    cerrado.lock();
    en_curso_.reset();
    hechas_.push_back(pedido.quien);
    hay_hechas_ = true;
    cerrado.unlock();
    despertador_.despertar();
    cerrado.lock();
  }
}

}
