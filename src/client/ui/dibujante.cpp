#include "client/ui/dibujante.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <iterator>

#include <d3d9.h>

namespace ventrik::ui {
namespace {
constexpr int kTriangulosPorTrazo = 32;
constexpr LONG kMargen = 1;
constexpr int kJuntar = 16;
constexpr int kVerticesDeMas = 5000;
constexpr int kIndicesDeMas = 10000;
constexpr DWORD kFormato = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
constexpr auto kBase = static_cast<std::size_t>(sizeof(std::size_t) == 8 ? 14695981039346656037ull : 2166136261ull);
constexpr auto kPrimo = static_cast<std::size_t>(sizeof(std::size_t) == 8 ? 1099511628211ull : 16777619ull);

static_assert(sizeof(ImDrawIdx) == 2, "los índices van en D3DFMT_INDEX16");

struct Vertice {
  float x;
  float y;
  float z;
  float rhw;
  D3DCOLOR color;
  float u;
  float v;
};

template <typename T>
std::size_t mezclar(std::size_t huella, const T& dato) {
  static_assert(sizeof(T) % sizeof(std::uint32_t) == 0, "se mezcla de a 4 bytes");
  const auto* palabras = reinterpret_cast<const std::uint32_t*>(&dato);
  for (std::size_t i = 0; i < sizeof(T) / sizeof(std::uint32_t); ++i) {
    huella = (huella ^ palabras[i]) * kPrimo;
  }
  return huella;
}

IDirect3DTexture9* a_textura(ImTextureID id) {
  return reinterpret_cast<IDirect3DTexture9*>(static_cast<std::uintptr_t>(id));
}

}

void Soltar::operator()(IUnknown* objeto) const { objeto->Release(); }

const std::vector<RECT>& Zonas::cambiadas(const ImDrawData& dibujo) {
  actuales_.clear();
  const RECT pantalla = {0, 0, static_cast<LONG>(dibujo.DisplaySize.x), static_cast<LONG>(dibujo.DisplaySize.y)};
  std::size_t numero = 0;
  for (const ImDrawList* lista : dibujo.CmdLists) {
    ++numero;
    for (const ImDrawCmd& orden : lista->CmdBuffer) {
      if (orden.UserCallback != nullptr) {
        continue;
      }
      const ImDrawVert* vertices = lista->VtxBuffer.Data + orden.VtxOffset;
      const ImDrawIdx* indices = lista->IdxBuffer.Data + orden.IdxOffset;
      const std::size_t base =
          mezclar(mezclar(mezclar(mezclar(kBase, numero), orden.ClipRect), orden.TexRef._TexData), orden.TexRef._TexID);
      std::size_t huella = base;
      ImVec2 menor(FLT_MAX, FLT_MAX);
      ImVec2 mayor(-FLT_MAX, -FLT_MAX);
      int triangulos = 0;
      for (unsigned int i = 0; i + 2 < orden.ElemCount; i += 3) {
        std::size_t triangulo = kBase;
        for (unsigned int k = i; k < i + 3; ++k) {
          const ImDrawVert& vertice = vertices[indices[k]];
          triangulo = mezclar(triangulo, vertice);
          menor = ImVec2(std::min(menor.x, vertice.pos.x), std::min(menor.y, vertice.pos.y));
          mayor = ImVec2(std::max(mayor.x, vertice.pos.x), std::max(mayor.y, vertice.pos.y));
        }
        huella = mezclar(huella, triangulo);
        if ((triangulo >> 3 & 7) != 0 && ++triangulos < kTriangulosPorTrazo && i + 5 < orden.ElemCount) {
          continue;
        }
        RECT caja = {static_cast<LONG>(std::floor(std::max(menor.x, orden.ClipRect.x))) - kMargen,
                     static_cast<LONG>(std::floor(std::max(menor.y, orden.ClipRect.y))) - kMargen,
                     static_cast<LONG>(std::ceil(std::min(mayor.x, orden.ClipRect.z))) + kMargen,
                     static_cast<LONG>(std::ceil(std::min(mayor.y, orden.ClipRect.w))) + kMargen};
        IntersectRect(&caja, &caja, &pantalla);
        actuales_.push_back({huella, caja});
        huella = base;
        menor = ImVec2(FLT_MAX, FLT_MAX);
        mayor = ImVec2(-FLT_MAX, -FLT_MAX);
        triangulos = 0;
      }
    }
  }
  zonas_.clear();
  const auto igual = [](const Trazo& antes, const Trazo& ahora) { return antes.huella == ahora.huella; };
  const auto [fin_antes, fin_ahora] =
      std::mismatch(anteriores_.begin(), anteriores_.end(), actuales_.begin(), actuales_.end(), igual);
  const auto [resto_antes, resto_ahora] =
      std::mismatch(anteriores_.rbegin(), std::make_reverse_iterator(fin_antes), actuales_.rbegin(),
                    std::make_reverse_iterator(fin_ahora), igual);
  const auto huellas = [](auto desde, auto hasta, std::vector<std::size_t>& salida) {
    salida.clear();
    std::transform(desde, hasta, std::back_inserter(salida), [](const Trazo& trazo) { return trazo.huella; });
    std::sort(salida.begin(), salida.end());
  };
  huellas(fin_antes, resto_antes.base(), huellas_antes_);
  huellas(fin_ahora, resto_ahora.base(), huellas_ahora_);
  const auto sumar_si_falta = [this](auto desde, auto hasta, const std::vector<std::size_t>& en) {
    std::for_each(desde, hasta, [&](const Trazo& trazo) {
      if (!std::binary_search(en.begin(), en.end(), trazo.huella)) {
        sumar(trazo.caja);
      }
    });
  };
  sumar_si_falta(fin_antes, resto_antes.base(), huellas_ahora_);
  sumar_si_falta(fin_ahora, resto_ahora.base(), huellas_antes_);
  if (zonas_.size() > kZonasMaximas) {
    RECT todo = zonas_.front();
    for (const RECT& zona : zonas_) {
      UnionRect(&todo, &todo, &zona);
    }
    zonas_.assign(1, todo);
  }
  std::swap(anteriores_, actuales_);
  return zonas_;
}

void Zonas::sumar(const RECT& caja) {
  if (IsRectEmpty(&caja)) {
    return;
  }
  for (RECT& zona : zonas_) {
    RECT cerca = zona;
    RECT cruce = {};
    InflateRect(&cerca, kJuntar, kJuntar);
    if (IntersectRect(&cruce, &cerca, &caja)) {
      UnionRect(&zona, &zona, &caja);
      return;
    }
  }
  zonas_.push_back(caja);
}

Dibujante::Dibujante(IDirect3DDevice9& dispositivo, int textura_maxima) : dispositivo_(dispositivo) {
  ImGuiIO& io = ImGui::GetIO();
  io.BackendRendererName = "ventrik";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
  ImGuiPlatformIO& plataforma = ImGui::GetPlatformIO();
  plataforma.Renderer_TextureMaxWidth = plataforma.Renderer_TextureMaxHeight = textura_maxima;
  preparar();
}

Dibujante::~Dibujante() {
  soltar();
  ImGuiIO& io = ImGui::GetIO();
  io.BackendRendererName = nullptr;
  io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
}

void Dibujante::preparar() {
  dispositivo_.SetPixelShader(nullptr);
  dispositivo_.SetVertexShader(nullptr);
  dispositivo_.SetFVF(kFormato);
  for (const auto& [estado, valor] : {std::pair<D3DRENDERSTATETYPE, DWORD>{D3DRS_CULLMODE, D3DCULL_NONE},
                                      {D3DRS_ZENABLE, D3DZB_FALSE},
                                      {D3DRS_LIGHTING, FALSE},
                                      {D3DRS_ALPHABLENDENABLE, TRUE},
                                      {D3DRS_SRCBLEND, D3DBLEND_SRCALPHA},
                                      {D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA},
                                      {D3DRS_SCISSORTESTENABLE, TRUE}}) {
    dispositivo_.SetRenderState(estado, valor);
  }
  for (const auto& [estado, valor] : {std::pair<D3DTEXTURESTAGESTATETYPE, DWORD>{D3DTSS_COLOROP, D3DTOP_MODULATE},
                                      {D3DTSS_COLORARG1, D3DTA_TEXTURE},
                                      {D3DTSS_COLORARG2, D3DTA_DIFFUSE},
                                      {D3DTSS_ALPHAOP, D3DTOP_MODULATE},
                                      {D3DTSS_ALPHAARG1, D3DTA_TEXTURE},
                                      {D3DTSS_ALPHAARG2, D3DTA_DIFFUSE}}) {
    dispositivo_.SetTextureStageState(0, estado, valor);
  }
  for (const auto& [estado, valor] : {std::pair<D3DSAMPLERSTATETYPE, DWORD>{D3DSAMP_MINFILTER, D3DTEXF_LINEAR},
                                      {D3DSAMP_MAGFILTER, D3DTEXF_LINEAR},
                                      {D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP},
                                      {D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP}}) {
    dispositivo_.SetSamplerState(0, estado, valor);
  }
  if (vertices_ && indices_) {
    dispositivo_.SetStreamSource(0, vertices_.get(), 0, sizeof(Vertice));
    dispositivo_.SetIndices(indices_.get());
  }
}

void Dibujante::soltar() {
  for (ImTextureData* textura : ImGui::GetPlatformIO().Textures) {
    if (textura->RefCount == 1) {
      textura->SetStatus(ImTextureStatus_WantDestroy);
      actualizar(*textura);
    }
  }
  vertices_.reset();
  indices_.reset();
  lugar_vertices_ = lugar_indices_ = 0;
}

void Dibujante::actualizar(ImTextureData& textura) {
  if (textura.Status == ImTextureStatus_WantDestroy) {
    if (IDirect3DTexture9* vieja = a_textura(textura.TexID)) {
      vieja->Release();
    }
    textura.SetTexID(ImTextureID_Invalid);
    textura.SetStatus(ImTextureStatus_Destroyed);
    return;
  }
  const bool crear = textura.Status == ImTextureStatus_WantCreate;
  if (crear) {
    IDirect3DTexture9* nueva = nullptr;
    if (FAILED(dispositivo_.CreateTexture(static_cast<UINT>(textura.Width), static_cast<UINT>(textura.Height), 1,
                                          D3DUSAGE_DYNAMIC,
                                          textura.Format == ImTextureFormat_Alpha8 ? D3DFMT_A8 : D3DFMT_A8R8G8B8,
                                          D3DPOOL_DEFAULT, &nueva, nullptr))) {
      return;
    }
    textura.SetTexID(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(nueva)));
  }
  const ImTextureRect todo = {0, 0, static_cast<unsigned short>(textura.Width),
                              static_cast<unsigned short>(textura.Height)};
  const ImTextureRect& zona = crear ? todo : textura.UpdateRect;
  const RECT bloqueo = {zona.x, zona.y, zona.x + zona.w, zona.y + zona.h};
  D3DLOCKED_RECT bloque = {};
  IDirect3DTexture9* destino = a_textura(textura.TexID);
  if (SUCCEEDED(destino->LockRect(0, &bloque, &bloqueo, 0))) {
    const auto copiar = [&](const ImTextureRect& parte) {
      for (int fila = parte.y; fila < parte.y + parte.h; ++fila) {
        std::copy_n(static_cast<const unsigned char*>(textura.GetPixelsAt(parte.x, fila)),
                    parte.w * textura.BytesPerPixel,
                    static_cast<unsigned char*>(bloque.pBits) + (fila - zona.y) * bloque.Pitch +
                        (parte.x - zona.x) * textura.BytesPerPixel);
      }
    };
    if (crear) {
      copiar(todo);
    } else {
      std::for_each(textura.Updates.begin(), textura.Updates.end(), copiar);
    }
    destino->UnlockRect(0);
  }
  textura.SetStatus(ImTextureStatus_OK);
}

bool Dibujante::llenar(const ImDrawData& dibujo) {
  if (lugar_vertices_ < dibujo.TotalVtxCount || lugar_indices_ < dibujo.TotalIdxCount) {
    vertices_.reset();
    indices_.reset();
    lugar_vertices_ = lugar_indices_ = 0;
    IDirect3DVertexBuffer9* vertices = nullptr;
    IDirect3DIndexBuffer9* indices = nullptr;
    const int cuantos_vertices = dibujo.TotalVtxCount + kVerticesDeMas;
    const int cuantos_indices = dibujo.TotalIdxCount + kIndicesDeMas;
    if (FAILED(dispositivo_.CreateVertexBuffer(static_cast<UINT>(cuantos_vertices) * sizeof(Vertice),
                                               D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, kFormato, D3DPOOL_DEFAULT,
                                               &vertices, nullptr))) {
      return false;
    }
    vertices_.reset(vertices);
    if (FAILED(dispositivo_.CreateIndexBuffer(static_cast<UINT>(cuantos_indices) * sizeof(ImDrawIdx),
                                              D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                              &indices, nullptr))) {
      vertices_.reset();
      return false;
    }
    indices_.reset(indices);
    lugar_vertices_ = cuantos_vertices;
    lugar_indices_ = cuantos_indices;
    dispositivo_.SetStreamSource(0, vertices, 0, sizeof(Vertice));
    dispositivo_.SetIndices(indices);
  }
  void* vertices = nullptr;
  void* indices = nullptr;
  if (FAILED(vertices_->Lock(0, static_cast<UINT>(dibujo.TotalVtxCount) * sizeof(Vertice), &vertices,
                             D3DLOCK_DISCARD))) {
    return false;
  }
  if (FAILED(indices_->Lock(0, static_cast<UINT>(dibujo.TotalIdxCount) * sizeof(ImDrawIdx), &indices,
                            D3DLOCK_DISCARD))) {
    vertices_->Unlock();
    return false;
  }
  auto* vertice = static_cast<Vertice*>(vertices);
  auto* indice = static_cast<ImDrawIdx*>(indices);
  rangos_.clear();
  for (const ImDrawList* lista : dibujo.CmdLists) {
    vertice = std::transform(lista->VtxBuffer.begin(), lista->VtxBuffer.end(), vertice, [](const ImDrawVert& v) {
      return Vertice{v.pos.x - 0.5f, v.pos.y - 0.5f, 0.0f, 1.0f, v.col, v.uv.x, v.uv.y};
    });
    indice = std::copy(lista->IdxBuffer.begin(), lista->IdxBuffer.end(), indice);
    for (const ImDrawCmd& orden : lista->CmdBuffer) {
      const ImDrawIdx* primero = lista->IdxBuffer.Data + orden.IdxOffset;
      const auto [menor, mayor] = std::minmax_element(primero, primero + orden.ElemCount);
      rangos_.emplace_back(orden.ElemCount == 0 ? 0u : *menor, orden.ElemCount == 0 ? 0u : *mayor - *menor + 1u);
    }
  }
  vertices_->Unlock();
  indices_->Unlock();
  return true;
}

void Dibujante::ordenes(const ImDrawData& dibujo, const RECT* zona) {
  INT base_vertices = 0;
  UINT base_indices = 0;
  std::size_t rango = 0;
  bool primera = true;
  IDirect3DTexture9* puesta = nullptr;
  DWORD operacion_puesta = D3DTOP_MODULATE;
  for (const ImDrawList* lista : dibujo.CmdLists) {
    for (const ImDrawCmd& orden : lista->CmdBuffer) {
      const auto [menor, cuantos] = rangos_[rango++];
      RECT recorte = {static_cast<LONG>(orden.ClipRect.x), static_cast<LONG>(orden.ClipRect.y),
                      static_cast<LONG>(orden.ClipRect.z), static_cast<LONG>(orden.ClipRect.w)};
      if (orden.UserCallback != nullptr || orden.ElemCount == 0 || IsRectEmpty(&recorte) ||
          (zona != nullptr && !IntersectRect(&recorte, &recorte, zona))) {
        continue;
      }
      const ImTextureData* datos = orden.TexRef._TexData;
      IDirect3DTexture9* textura = a_textura(datos != nullptr ? datos->TexID : orden.TexRef._TexID);
      const DWORD operacion =
          datos != nullptr && datos->Format == ImTextureFormat_Alpha8 ? D3DTOP_SELECTARG2 : D3DTOP_MODULATE;
      if (primera || textura != puesta) {
        dispositivo_.SetTexture(0, textura);
        puesta = textura;
      }
      if (primera || operacion != operacion_puesta) {
        dispositivo_.SetTextureStageState(0, D3DTSS_COLOROP, operacion);
        operacion_puesta = operacion;
      }
      primera = false;
      dispositivo_.SetScissorRect(&recorte);
      dispositivo_.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, base_vertices + static_cast<INT>(orden.VtxOffset), menor,
                                        cuantos, base_indices + orden.IdxOffset, orden.ElemCount / 3);
    }
    base_vertices += lista->VtxBuffer.Size;
    base_indices += static_cast<UINT>(lista->IdxBuffer.Size);
  }
}

void Dibujante::dibujar(const ImDrawData& dibujo, const std::vector<RECT>& zonas) {
  if (dibujo.Textures != nullptr) {
    for (ImTextureData* textura : *dibujo.Textures) {
      if (textura->Status != ImTextureStatus_OK) {
        actualizar(*textura);
      }
    }
  }
  if (dibujo.TotalVtxCount == 0 || !llenar(dibujo)) {
    return;
  }
  if (zonas.empty()) {
    ordenes(dibujo, nullptr);
  }
  for (const RECT& zona : zonas) {
    ordenes(dibujo, &zona);
  }
}

}
