// Rebuild with: swift tools/generate-accountbank-icon.swift
// A simple account list, deliberately distinct from Telegram's paper plane.
import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

let root = URL(fileURLWithPath: #filePath)
    .deletingLastPathComponent()
    .deletingLastPathComponent()
let assets = root.appendingPathComponent("Telegram/Telegram/Images.xcassets")
let iconset = assets.appendingPathComponent("AccountBank.iconset")
let appiconset = assets.appendingPathComponent("AccountBank.appiconset")
let compiledIcon = root.appendingPathComponent("Telegram/Resources/AccountBank/Icon.icns")
try FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
try FileManager.default.createDirectory(at: appiconset, withIntermediateDirectories: true)
try FileManager.default.createDirectory(at: compiledIcon.deletingLastPathComponent(), withIntermediateDirectories: true)

func color(_ red: CGFloat, _ green: CGFloat, _ blue: CGFloat, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(red: red / 255, green: green / 255, blue: blue / 255, alpha: alpha)
}

func rounded(_ context: CGContext, _ rect: CGRect, _ radius: CGFloat, _ fill: CGColor) {
    context.addPath(CGPath(roundedRect: rect, cornerWidth: radius, cornerHeight: radius, transform: nil))
    context.setFillColor(fill)
    context.fillPath()
}

func render(_ pixels: Int) -> CGImage {
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    let context = CGContext(
        data: nil,
        width: pixels,
        height: pixels,
        bitsPerComponent: 8,
        bytesPerRow: 0,
        space: colorSpace,
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
    )!
    context.interpolationQuality = .high
    context.scaleBy(x: CGFloat(pixels) / 1024, y: CGFloat(pixels) / 1024)

    let shell = CGRect(x: 48, y: 48, width: 928, height: 928)
    context.saveGState()
    context.addPath(CGPath(roundedRect: shell, cornerWidth: 210, cornerHeight: 210, transform: nil))
    context.clip()
    let gradient = CGGradient(
        colorsSpace: colorSpace,
        colors: [color(18, 38, 55), color(20, 84, 91)] as CFArray,
        locations: [0, 1]
    )!
    context.drawLinearGradient(gradient, start: CGPoint(x: 90, y: 900), end: CGPoint(x: 940, y: 90), options: [])
    context.restoreGState()

    // Three account rows remain legible in the small Dock icon.
    for (index, y) in [602.0, 423.0, 244.0].enumerated() {
        let row = CGRect(x: 192, y: y, width: 640, height: 152)
        rounded(context, row, 47, color(232, 249, 246, 0.94))
        let dot = CGRect(x: 233, y: y + 37, width: 78, height: 78)
        rounded(context, dot, 39, index == 0 ? color(41, 173, 153) : color(83, 130, 141))
        rounded(context, CGRect(x: 353, y: y + 82, width: index == 1 ? 280 : 330, height: 20), 10, color(38, 66, 78, 0.84))
        rounded(context, CGRect(x: 353, y: y + 47, width: index == 2 ? 215 : 260, height: 16), 8, color(91, 127, 134, 0.51))
    }
    return context.makeImage()!
}

func save(_ image: CGImage, to url: URL) {
    let destination = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(destination, image, nil)
    precondition(CGImageDestinationFinalize(destination), "Could not write \(url.path)")
}

let iconsetNames: [(String, Int)] = [
    ("icon_16x16.png", 16), ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32), ("icon_32x32@2x.png", 64),
    ("icon_128x128.png", 128), ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256), ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512), ("icon_512x512@2x.png", 1024)
]
let appiconNames: [(String, Int)] = [
    ("icon16.png", 16), ("icon16@2x.png", 32),
    ("icon32.png", 32), ("icon32@2x.png", 64),
    ("icon128.png", 128), ("icon128@2x.png", 256),
    ("icon256.png", 256), ("icon256@2x.png", 512),
    ("icon512.png", 512), ("icon512@2x.png", 1024)
]
for ((iconsetName, size), (appiconName, _)) in zip(iconsetNames, appiconNames) {
    let image = render(size)
    save(image, to: iconset.appendingPathComponent(iconsetName))
    save(image, to: appiconset.appendingPathComponent(appiconName))
}
let contents: [String: Any] = [
    "images": [16, 32, 128, 256, 512].flatMap { size in
        [1, 2].map { scale in
            ["size": "\(size)x\(size)", "idiom": "mac", "filename": "icon\(size)\(scale == 2 ? "@2x" : "").png", "scale": "\(scale)x"]
        }
    },
    "info": ["version": 1, "author": "AccountBank"]
]
let json = try JSONSerialization.data(withJSONObject: contents, options: [.prettyPrinted, .sortedKeys])
try json.write(to: appiconset.appendingPathComponent("Contents.json"))
// ICNS stores PNG payloads in big-endian chunks. This avoids iconutil's
// "Invalid Iconset" failure on some macOS/File Provider workspaces.
func bigEndian(_ number: UInt32) -> Data {
    withUnsafeBytes(of: number.bigEndian) { Data($0) }
}
var icns = Data("icns".utf8)
icns.append(bigEndian(0))
for (type, file) in [
    ("icp4", "icon_16x16.png"),
    ("icp5", "icon_16x16@2x.png"),
    ("icp6", "icon_32x32@2x.png"),
    ("ic07", "icon_128x128.png"),
    ("ic08", "icon_256x256.png"),
    ("ic09", "icon_512x512.png"),
    ("ic10", "icon_512x512@2x.png")
] {
    let png = try Data(contentsOf: iconset.appendingPathComponent(file))
    icns.append(contentsOf: type.utf8)
    icns.append(bigEndian(UInt32(png.count + 8)))
    icns.append(png)
}
icns.replaceSubrange(4..<8, with: bigEndian(UInt32(icns.count)))
try icns.write(to: compiledIcon)
print("Generated AccountBank icon assets")
