#include <string>

#include <doctest/doctest.h>
#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnperrors.h>
#include <upnpreplyparse.h>

TEST_CASE("miniupnpc lee la respuesta de un router") {
  const std::string respuesta =
      "<?xml version=\"1.0\"?>"
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
      "<u:GetExternalIPAddressResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
      "<NewExternalIPAddress>203.0.113.7</NewExternalIPAddress>"
      "</u:GetExternalIPAddressResponse></s:Body></s:Envelope>";
  NameValueParserData datos;
  ParseNameValue(respuesta.data(), static_cast<int>(respuesta.size()), &datos);
  const char* ip = GetValueFromNameValueList(&datos, "NewExternalIPAddress");
  REQUIRE(ip != nullptr);
  CHECK(std::string(ip) == "203.0.113.7");
  ClearNameValueList(&datos);
}

TEST_CASE("miniupnpc conoce los errores de UPnP") {
  const char* mensaje = strupnperror(402);
  REQUIRE(mensaje != nullptr);
  CHECK(std::string(mensaje) == "Invalid Args");
}

TEST_CASE("las funciones de red de miniupnpc quedan dentro del programa") {
  // No se llaman porque necesitan la red. Se nombran para que queden
  // dentro del .exe y check.ps1 revise que lo que usan exista en XP.
  decltype(&upnpDiscover) volatile descubrir = &upnpDiscover;
  decltype(&UPNP_GetValidIGD) volatile buscar_router = &UPNP_GetValidIGD;
  decltype(&UPNP_AddPortMapping) volatile abrir_puerto = &UPNP_AddPortMapping;
  CHECK(descubrir != nullptr);
  CHECK(buscar_router != nullptr);
  CHECK(abrir_puerto != nullptr);
}
