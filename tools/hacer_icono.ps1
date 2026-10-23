# Ventrik: arma el ícono del programa (src\client\ventrik.ico) desde docs\logo.png.
#
# Se queda solo con la figura (la mancha más grande que no es transparente),
# la recorta en un cuadrado y guarda los tamaños que usa Windows. Hasta 128
# van como mapa de bits de 32 bits, que XP entiende; el de 256 va en PNG,
# como lo leen Vista y los Windows más nuevos.
# Se corre a mano solo cuando cambia el logo.
#
# Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\hacer_icono.ps1

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$raiz = Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class Icono {
    static byte[] Bytes(Bitmap imagen, out int paso) {
        var datos = imagen.LockBits(new Rectangle(0, 0, imagen.Width, imagen.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        paso = datos.Stride;
        var bytes = new byte[datos.Stride * imagen.Height];
        Marshal.Copy(datos.Scan0, bytes, 0, bytes.Length);
        imagen.UnlockBits(datos);
        return bytes;
    }

    public static string Armar(string origen, string destino, int[] lados) {
        var logo = new Bitmap(origen);
        int ancho = logo.Width, alto = logo.Height, paso;
        var p = Bytes(logo, out paso);
        var figura = new bool[ancho * alto];
        var pendientes = new Stack<int>();
        pendientes.Push(alto / 2 * ancho + ancho / 2);
        int x0 = ancho, y0 = alto, x1 = 0, y1 = 0, sueltos = 0;
        while (pendientes.Count > 0) {
            int i = pendientes.Pop();
            int x = i % ancho, y = i / ancho;
            if (figura[i] || p[y * paso + x * 4 + 3] < 32) continue;
            figura[i] = true;
            x0 = Math.Min(x0, x); y0 = Math.Min(y0, y); x1 = Math.Max(x1, x); y1 = Math.Max(y1, y);
            if (x > 0) pendientes.Push(i - 1);
            if (x < ancho - 1) pendientes.Push(i + 1);
            if (y > 0) pendientes.Push(i - ancho);
            if (y < alto - 1) pendientes.Push(i + ancho);
        }
        var limpio = new Bitmap(ancho, alto, PixelFormat.Format32bppArgb);
        var datos = limpio.LockBits(new Rectangle(0, 0, ancho, alto), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        var q = new byte[datos.Stride * alto];
        for (int y = 0; y < alto; y++) {
            for (int x = 0; x < ancho; x++) {
                bool queda = figura[y * ancho + x];
                for (int k = 0; k < 4; k++) q[y * datos.Stride + x * 4 + k] = queda ? p[y * paso + x * 4 + k] : (byte)0;
                if (!queda && p[y * paso + x * 4 + 3] >= 32) sueltos++;
            }
        }
        Marshal.Copy(q, 0, datos.Scan0, q.Length);
        limpio.UnlockBits(datos);

        int lado = Math.Max(x1 - x0, y1 - y0) + 1;
        lado += lado / 50 * 2;
        var cuadro = new Rectangle((x0 + x1 + 1 - lado) / 2, (y0 + y1 + 1 - lado) / 2, lado, lado);

        var entradas = new List<byte[]>();
        foreach (int tamano in lados) {
            var chica = new Bitmap(tamano, tamano, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(chica)) {
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                g.CompositingQuality = CompositingQuality.HighQuality;
                g.Clear(Color.Transparent);
                g.DrawImage(limpio, new Rectangle(0, 0, tamano, tamano), cuadro, GraphicsUnit.Pixel);
            }
            if (tamano >= 256) {
                using (var png = new MemoryStream()) { chica.Save(png, ImageFormat.Png); entradas.Add(png.ToArray()); }
                continue;
            }
            int pasoChica;
            var c = Bytes(chica, out pasoChica);
            int pasoMascara = (tamano + 31) / 32 * 4;
            using (var bmp = new MemoryStream())
            using (var w = new BinaryWriter(bmp)) {
                w.Write(40); w.Write(tamano); w.Write(tamano * 2); w.Write((short)1); w.Write((short)32);
                w.Write(0); w.Write(tamano * tamano * 4 + pasoMascara * tamano); w.Write(0); w.Write(0); w.Write(0); w.Write(0);
                for (int y = tamano - 1; y >= 0; y--) w.Write(c, y * pasoChica, tamano * 4);
                for (int y = tamano - 1; y >= 0; y--) {
                    var fila = new byte[pasoMascara];
                    for (int x = 0; x < tamano; x++) if (c[y * pasoChica + x * 4 + 3] == 0) fila[x / 8] |= (byte)(0x80 >> (x % 8));
                    w.Write(fila);
                }
                w.Flush();
                entradas.Add(bmp.ToArray());
            }
        }

        using (var ico = new FileStream(destino, FileMode.Create))
        using (var w = new BinaryWriter(ico)) {
            w.Write((short)0); w.Write((short)1); w.Write((short)lados.Length);
            int desplazamiento = 6 + 16 * lados.Length;
            for (int i = 0; i < lados.Length; i++) {
                w.Write((byte)(lados[i] >= 256 ? 0 : lados[i])); w.Write((byte)(lados[i] >= 256 ? 0 : lados[i]));
                w.Write((byte)0); w.Write((byte)0); w.Write((short)1); w.Write((short)32);
                w.Write(entradas[i].Length); w.Write(desplazamiento);
                desplazamiento += entradas[i].Length;
            }
            foreach (var entrada in entradas) w.Write(entrada);
        }
        return String.Format("figura {0},{1}-{2},{3}; recorte {4}; pixeles sueltos quitados: {5}", x0, y0, x1, y1, cuadro, sueltos);
    }
}
"@

$destino = Join-Path $raiz 'src\client\ventrik.ico'
[Icono]::Armar((Join-Path $raiz 'docs\logo.png'), $destino, @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256))
Write-Host ('Listo: ' + $destino + ' (' + (Get-Item $destino).Length + ' bytes)')
