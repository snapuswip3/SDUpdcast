using BsDiff;
using LazyCache;
using System.Security.Cryptography;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddRazorPages();
builder.Services.AddLazyCache();

var app = builder.Build();

app.UseStaticFiles();
app.MapRazorPages();

app.MapGet("/dc/{name}", (HttpContext ctx, string name, IAppCache cache, IWebHostEnvironment env) =>
{
    var dir = Path.Combine(env.WebRootPath, "dc");

    if (!Directory.Exists(dir))
        return Results.NotFound();

    var baseName = Path.GetFileNameWithoutExtension(name);
    var files = Directory.GetFiles(dir, $"{baseName}*.bin");

    if (files.Length == 0)
        return Results.NotFound();

    var latest = files
        .Select(f => new { Path = f, Version = ExtractVersionNumber(f, baseName) })
        .OrderByDescending(x => x.Version)
        .ThenByDescending(x => x.Path)
        .First()
        .Path;

    string latestMd5 = cache.GetOrAdd(latest, entry =>
    {
        entry.SlidingExpiration = TimeSpan.FromMinutes(10);

        using var s = File.OpenRead(latest);
        return ToMd5(s);
    });

    var clientMd5 = ctx.Request.Query["md5"].ToString();

    ctx.Response.Headers["X-Update-MD5"] = latestMd5;

    if (!string.IsNullOrEmpty(clientMd5) &&
        clientMd5.Equals(latestMd5, StringComparison.OrdinalIgnoreCase))
    {
        ctx.Response.StatusCode = 204;
        return Results.Empty;
    }

    string? baseFile = null;

    if (!string.IsNullOrEmpty(clientMd5))
    {
        foreach (var f in files)
        {
            var md5 = cache.GetOrAdd(f, entry =>
            {
                entry.SlidingExpiration = TimeSpan.FromMinutes(10);

                using var s = File.OpenRead(f);
                return ToMd5(s);
            });

            if (md5.Equals(clientMd5, StringComparison.OrdinalIgnoreCase))
            {
                baseFile = f;
                break;
            }
        }
    }

    if (baseFile is not null && baseFile != latest)
    {
        var cacheKey = $"patch:{clientMd5}->{latestMd5}";

        var patchData = cache.GetOrAdd(cacheKey, entry =>
        {
            entry.AbsoluteExpirationRelativeToNow = TimeSpan.FromMinutes(30);

            var oldBytes = File.ReadAllBytes(baseFile);
            var newBytes = File.ReadAllBytes(latest);

            using var ms = new MemoryStream();
            BinaryPatch.Create(oldBytes, newBytes, ms);

            return ms.ToArray();
        });

        var fullSize = new FileInfo(latest).Length;

        if (patchData.Length < fullSize * 0.8)
        {
            ctx.Response.Headers["X-Patched-Size"] = fullSize.ToString();
            return Results.File(patchData, "application/octet-stream");
        }
    }

    return Results.File(latest, "application/octet-stream");
});

app.Run();

static int ExtractVersionNumber(string path, string baseName)
{
    var file = Path.GetFileNameWithoutExtension(path);

    if (file.Length == baseName.Length)
        return 0;

    var suffix = file[baseName.Length..];

    return int.TryParse(suffix, out var v) ? v : 0;
}

static string ToMd5(Stream stream)
{
    var hash = MD5.HashData(stream);
    return BitConverter.ToString(hash)
        .Replace("-", "")
        .ToLowerInvariant();
}