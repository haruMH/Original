using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

namespace TextureGenerator
{
    class Program
    {
        static float Clamp(float val, float min, float max)
        {
            if (val < min) return min;
            if (val > max) return max;
            return val;
        }

        static void Main(string[] args)
        {
            string srcPath = @"C:\Users\HARU17\.gemini\antigravity-ide\brain\24e6e578-346c-4e14-b296-f2d456143b23\wall_comic_block_1789385788595.jpg";
            if (!File.Exists(srcPath))
            {
                Console.WriteLine("Source file not found: " + srcPath);
                return;
            }

            using (Bitmap bmpOrig = new Bitmap(srcPath))
            {
                int w = bmpOrig.Width;
                int h = bmpOrig.Height;
                int bytes = w * h * 4;

                using (Bitmap bmpCyan = new Bitmap(w, h, PixelFormat.Format32bppArgb))
                using (Bitmap bmpGold = new Bitmap(w, h, PixelFormat.Format32bppArgb))
                using (Bitmap bmpCrim = new Bitmap(w, h, PixelFormat.Format32bppArgb))
                {
                    Rectangle rect = new Rectangle(0, 0, w, h);
                    BitmapData dataOrig = bmpOrig.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                    BitmapData dataCyan = bmpCyan.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                    BitmapData dataGold = bmpGold.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                    BitmapData dataCrim = bmpCrim.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);

                    byte[] bufOrig = new byte[bytes];
                    byte[] bufCyan = new byte[bytes];
                    byte[] bufGold = new byte[bytes];
                    byte[] bufCrim = new byte[bytes];

                    Marshal.Copy(dataOrig.Scan0, bufOrig, 0, bytes);

                    for (int i = 0; i < bytes; i += 4)
                    {
                        float b = bufOrig[i];
                        float g = bufOrig[i + 1];
                        float r = bufOrig[i + 2];

                        float lum = 0.299f * r + 0.587f * g + 0.114f * b;
                        // 目地（インクの黒い溝）の検出
                        float edge = Math.Max(0.0f, (60.0f - lum) / 60.0f);
                        // ハイライト（上面の角やエッジ）の検出
                        float hi = Math.Max(0.0f, (lum - 75.0f) / 100.0f);

                        // ----------------------------------------------------
                        // 1. シアン・ルーン（ネオンブルー・冷徹な魔導石）
                        // ----------------------------------------------------
                        float r1 = Clamp(r * 0.45f + edge * 20.0f + hi * 40.0f, 0f, 255f);
                        float g1 = Clamp(g * 0.90f + edge * 210.0f + hi * 170.0f, 0f, 255f);
                        float b1 = Clamp(b * 1.15f + edge * 255.0f + hi * 255.0f, 0f, 255f);
                        bufCyan[i]     = (byte)b1;
                        bufCyan[i + 1] = (byte)g1;
                        bufCyan[i + 2] = (byte)r1;
                        bufCyan[i + 3] = 255;

                        // ----------------------------------------------------
                        // 2. ゴールド・エンバー（琥珀・金色の神殿風トゥーン石）
                        // ----------------------------------------------------
                        float r2 = Clamp(r * 1.30f + edge * 255.0f + hi * 220.0f, 0f, 255f);
                        float g2 = Clamp(g * 1.05f + edge * 165.0f + hi * 140.0f, 0f, 255f);
                        float b2 = Clamp(b * 0.35f + edge * 15.0f  + hi * 20.0f, 0f, 255f);
                        bufGold[i]     = (byte)b2;
                        bufGold[i + 1] = (byte)g2;
                        bufGold[i + 2] = (byte)r2;
                        bufGold[i + 3] = 255;

                        // ----------------------------------------------------
                        // 3. クリムゾン・ブラッド（紅蓮・ボスの闘技場風トゥーン石）
                        // ----------------------------------------------------
                        float r3 = Clamp(r * 1.35f + edge * 255.0f + hi * 190.0f, 0f, 255f);
                        float g3 = Clamp(g * 0.45f + edge * 30.0f  + hi * 20.0f, 0f, 255f);
                        float b3 = Clamp(b * 0.65f + edge * 80.0f  + hi * 60.0f, 0f, 255f);
                        bufCrim[i]     = (byte)b3;
                        bufCrim[i + 1] = (byte)g3;
                        bufCrim[i + 2] = (byte)r3;
                        bufCrim[i + 3] = 255;
                    }

                    Marshal.Copy(bufCyan, 0, dataCyan.Scan0, bytes);
                    Marshal.Copy(bufGold, 0, dataGold.Scan0, bytes);
                    Marshal.Copy(bufCrim, 0, dataCrim.Scan0, bytes);

                    bmpOrig.UnlockBits(dataOrig);
                    bmpCyan.UnlockBits(dataCyan);
                    bmpGold.UnlockBits(dataGold);
                    bmpCrim.UnlockBits(dataCrim);

                    string outDir = @"Assets\texture";
                    Directory.CreateDirectory(outDir);

                    string pCyan = Path.Combine(outDir, "wall_comic_cyan.png");
                    string pGold = Path.Combine(outDir, "wall_comic_gold.png");
                    string pCrim = Path.Combine(outDir, "wall_comic_crimson.png");

                    bmpCyan.Save(pCyan, ImageFormat.Png);
                    bmpGold.Save(pGold, ImageFormat.Png);
                    bmpCrim.Save(pCrim, ImageFormat.Png);

                    Console.WriteLine("Saved textures to " + outDir);

                    string[] dirs = { @"x64\Debug", @"x64\Release\Assets\texture" };
                    foreach (var d in dirs)
                    {
                        Directory.CreateDirectory(d);
                        File.Copy(pCyan, Path.Combine(d, "wall_comic_cyan.png"), true);
                        File.Copy(pGold, Path.Combine(d, "wall_comic_gold.png"), true);
                        File.Copy(pCrim, Path.Combine(d, "wall_comic_crimson.png"), true);
                    }
                    Console.WriteLine("Copied textures to output directories.");
                }
            }
        }
    }
}
