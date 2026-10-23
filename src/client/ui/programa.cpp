#include "client/ui/programa.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <variant>

#include "client/guardados.h"
#include "client/sesion.h"
#include "client/ui/inicio.h"
#include "client/ui/ventana.h"
#include "common/cifrado/inicio.h"
#include "common/texto.h"
#include "common/utf.h"

namespace ventrik::ui {
namespace {
using namespace std::chrono_literals;

class Programa {
 public:
  Programa(Idioma idioma, HWND ventana, ImTextureID logo) : ventana_(ventana) {
    pantalla_.idioma = idioma;
    pantalla_.logo = logo;
    const cliente::LecturaGuardados lectura = cliente::cargar_guardados(guardados_);
    se_puede_guardar_ = lectura == cliente::LecturaGuardados::leidos || lectura == cliente::LecturaGuardados::danados;
    const Clave aviso = lectura == cliente::LecturaGuardados::version_nueva ? Clave::inicio_lista_nueva
                        : lectura == cliente::LecturaGuardados::error   ? Clave::inicio_lista_ilegible
                        : lectura == cliente::LecturaGuardados::danados ? Clave::inicio_lista_danada
                                                                        : Clave::titulo_ventana;
    if (aviso != Clave::titulo_ventana) {
      pantalla_.aviso = texto(idioma, aviso);
    }
    poner(pantalla_.nombre, guardados_.nombre);
  }

  void atender() {
    avisado_ = false;
    if (sesion_) {
      for (const cliente::Novedad& novedad : sesion_->esperar_novedades(0ms)) {
        if (const auto* intento = std::get_if<cliente::IntentoDeConexion>(&novedad)) {
          conectado(*intento);
        } else if (const auto* fin = std::get_if<cliente::Fin>(&novedad)) {
          terminar(cliente::explicar(pantalla_.idioma, *fin));
        }
      }
    }
  }

  void cuadro() {
    const Pedido pedido = dibujar_inicio(pantalla_, guardados_);
    switch (pedido.accion) {
      case Accion::entrar_con_formulario:
        entrar_con_formulario();
        break;
      case Accion::entrar_a_guardado: {
        const cliente::ServidorGuardado& servidor = guardados_.servidores[pedido.cual];
        entrar(servidor.direccion, cifrado::Huella(servidor.huella), {servidor.nombre, std::nullopt}, false);
        break;
      }
      case Accion::quitar:
        if (pantalla_.intento.direccion == guardados_.servidores[pedido.cual].direccion) {
          desconectar();
        }
        guardados_.servidores.erase(guardados_.servidores.begin() + static_cast<std::ptrdiff_t>(pedido.cual));
        guardar();
        break;
      case Accion::desconectar:
        desconectar();
        break;
      case Accion::ninguna:
        break;
    }
  }

 private:
  void entrar_con_formulario() {
    const auto direccion = red::leer_direccion(pantalla_.direccion.data());
    const auto huella = cifrado::leer_huella(pantalla_.huella.data());
    cliente::Presentacion quien{pantalla_.nombre.data(), std::nullopt};
    const bool con_codigo = pantalla_.codigo[0] != '\0';
    if (con_codigo) {
      quien.codigo_dueno = cifrado::leer_codigo_dueno(pantalla_.codigo.data());
    }
    const Clave problema = !direccion                          ? Clave::conectar_direccion_invalida
                           : !huella                           ? Clave::conectar_huella_invalida
                           : !es_nombre_valido(quien.nombre)   ? Clave::conectar_nombre_invalido
                           : con_codigo && !quien.codigo_dueno ? Clave::conectar_codigo_invalido
                                                               : Clave::titulo_ventana;
    pantalla_.error_formulario.clear();
    if (problema != Clave::titulo_ventana) {
      pantalla_.error_formulario = texto(pantalla_.idioma, problema);
      return;
    }
    entrar(*direccion, *huella, std::move(quien), true);
  }

  void entrar(const red::Direccion& direccion, const cifrado::Huella& huella, cliente::Presentacion quien,
              bool desde_formulario) {
    sesion_.reset();
    pantalla_.intento = {direccion, desde_formulario, Fase::conectando, {}};
    pendiente_ = {direccion, huella.bytes(), quien.nombre, 0, {}, 0};
    cliente::OpcionesSesion opciones;
    opciones.al_haber_novedades = [this] {
      if (!avisado_.exchange(true)) {
        PostMessageW(ventana_, kMensajeNovedades, 0, 0);
      }
    };
    sesion_ = std::make_unique<cliente::Sesion>(direccion, huella, std::move(quien), std::move(opciones));
  }

  void conectado(const cliente::IntentoDeConexion& intento) {
    if (intento.estado != cliente::EstadoConexion::conectado || !intento.presentado) {
      terminar(cliente::explicar(pantalla_.idioma, intento));
      return;
    }
    const protocolo::Presentado& presentado = *intento.presentado;
    const auto yo = std::find_if(presentado.usuarios.begin(), presentado.usuarios.end(),
                                 [&presentado](const protocolo::UsuarioInfo& usuario) {
                                   return usuario.id == presentado.tu_id;
                                 });
    if (yo != presentado.usuarios.end()) {
      const auto canal = std::find_if(presentado.canales.begin(), presentado.canales.end(),
                                      [&yo](const protocolo::CanalInfo& cual) { return cual.id == yo->canal; });
      pendiente_.canal = yo->canal;
      pendiente_.nombre_canal = canal != presentado.canales.end() ? canal->nombre : std::string();
    }
    pendiente_.ultima_vez_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    guardados_.nombre = pendiente_.nombre;
    if (cliente::recordar(guardados_, pendiente_)) {
      guardar();
    }
    pantalla_.intento.fase = Fase::conectada;
    pantalla_.intento.mensaje = con_datos(texto(pantalla_.idioma, Clave::inicio_conectado_como), {pendiente_.nombre});
    pantalla_.codigo.fill('\0');
    if (pantalla_.intento.desde_formulario) {
      pantalla_.direccion.fill('\0');
      pantalla_.huella.fill('\0');
    }
  }

  void terminar(std::string mensaje) {
    sesion_.reset();
    pantalla_.intento.fase = Fase::error;
    pantalla_.intento.mensaje = std::move(mensaje);
  }

  void desconectar() {
    sesion_.reset();
    pantalla_.intento = {};
  }

  void guardar() {
    if (se_puede_guardar_) {
      pantalla_.aviso = cliente::guardar_guardados(guardados_)
                            ? std::string()
                            : std::string(texto(pantalla_.idioma, Clave::inicio_no_se_guardo));
    }
  }

  const HWND ventana_;
  std::atomic<bool> avisado_{false};
  PantallaInicio pantalla_;
  cliente::Guardados guardados_;
  bool se_puede_guardar_ = false;
  cliente::ServidorGuardado pendiente_;
  std::unique_ptr<cliente::Sesion> sesion_;
};

}

int abrir_programa(Idioma idioma, const std::wstring& titulo, int como_mostrar) {
  const cifrado::Inicio cifrado;
  const red::Inicio red;
  Ventana ventana(titulo, como_mostrar);
  if (!ventana.ok()) {
    MessageBoxW(nullptr, a_utf16(texto(idioma, Clave::aviso_sin_direct3d)).c_str(), titulo.c_str(),
                MB_OK | MB_ICONERROR);
    return 3;
  }
  Programa programa(idioma, ventana.hwnd(), ventana.logo());
  return ventana.correr([&programa] { programa.atender(); }, [&programa] { programa.cuadro(); });
}

}
