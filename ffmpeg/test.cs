using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Xml;
using System.Xml.Serialization;

public class codec
{
	[DllImport("codec.dll")]
	public static extern int version();

	[DllImport("codec.dll")]
	public static extern void init();

	[DllImport("codec.dll")]
	public static extern int resolution(IntPtr data);

	[DllImport("codec.dll")]
	public static extern int channels(IntPtr data);

	[DllImport("codec.dll")]
	public static extern IntPtr encode_open_output(string file, string codec_name,
			string pix_fmt_name, int resolution, int channels);

	[DllImport("codec.dll")]
	public static extern int encode_write_frame(IntPtr data, byte[] frame, int channels);

	[DllImport("codec.dll")]
	public static extern void encode_close(IntPtr data);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_open_input(string file);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_read_frame(IntPtr data, out int got);

	[DllImport("codec.dll")]
	public static extern void decode_close(IntPtr data);
}

[XmlRoot("Vixen3_Export")]
public class Export
{
	[XmlElement("Resolution")]
	public ulong Resolution { get; set; }
	[XmlElement("OutFile")]
	public string OutFile { get; set; }
	[XmlElement("Duration")]
	public string Duration { get; set; }
	[XmlArray("Network")]
	public List<Controller> Network { get; set; }
	[XmlArray("Media")]
	[XmlArrayItem("FilePath")]
	public List<string> Media { get; set; }
}

[XmlType("Controller")]
public class Controller
{
	[XmlElement("Index")]
	public int Index { get; set; }
	[XmlElement("Name")]
	public string Name { get; set; }
	[XmlElement("StartChan")]
	public int StartChan { get; set; }
	[XmlElement("Channels")]
	public int Channels { get; set; }
}

public class HelloWorld
{
	[STAThread]
	private static int Main(string[] args)
	{
		Console.WriteLine("libcodec version: " + codec.version());

		if (args.Length != 2) {
			Console.WriteLine("usage: test.exe input output");
			return 1;
		}
		string input = args[0], output = args[1];
		bool enc = input.EndsWith(".xml", true, null);
		Console.WriteLine((enc ? "Encoding " : "Decoding ") + input + " to " + output);

		codec.init();
		if (enc)
			return encode(input, output);
		else
			return decode(input, output);
	}

	// 4 bytes header, 1 byte command (set frame), 1 byte stream
	private static byte[] header = { 0xde, 0xad, 0xbe, 0xef, 0x02, 0x00 };

	private static UInt16 ReadHeader(BinaryReader br)
	{
		var hdr = br.ReadBytes(header.Length + 2);
		for (int i = 0; i != header.Length; i++)
			if (hdr[i] != header[i])
				throw new Exception("Frame header error @" +
						(br.BaseStream.Position - header.Length + i));
		return (UInt16)(hdr[header.Length] | (hdr[header.Length + 1] << 8));
	}

	private static byte[] ReadFrame(BinaryReader br)
	{
		UInt16 channels = ReadHeader(br);
		return br.ReadBytes(channels);
	}

	private static int encode(string input, string output)
	{
		// Deserialise XML controller configuration
		var serializer = new XmlSerializer(typeof(Export));
		XmlReader reader = XmlReader.Create(input);
		Export export = (Export)serializer.Deserialize(reader);
		reader.Close();

		var con = export.Network.Last();
		int channels = con.StartChan + con.Channels - 1;

		// Open video file for encoding output
		IntPtr data = codec.encode_open_output(output, "libx264rgb", "rgb24",
				(int)export.Resolution, channels);
		if (data == IntPtr.Zero)
			return 1;

		// Open raw file for encoding input
		BinaryReader br = new BinaryReader(File.OpenRead(export.OutFile));
		try {
			for (;;) {
				byte[] frame = ReadFrame(br);
				if (codec.encode_write_frame(data, frame, channels) == 0)
					break;
			}
		} catch (Exception e) {
			Console.WriteLine("Exception: " + e.ToString());
		}

		// Close files
		codec.encode_close(data);
		br.Close();
		return 0;
	}

	private static int decode(string input, string output)
	{
		// Open video file for decoding input
		IntPtr data = codec.decode_open_input(input);
		if (data == IntPtr.Zero)
			return 1;
		int channels = codec.channels(data);
		byte[] chdata = new byte[channels];
		Console.WriteLine(channels + " channels available from " + input);

		// Read video frames and write to binary file
		BinaryWriter bw = new BinaryWriter(File.Create(output));
		int got = 0;
		for (;;) {
			IntPtr frame = codec.decode_read_frame(data, out got);
			if (got == 0)
				break;
			if (frame != IntPtr.Zero)
				Marshal.Copy(frame, chdata, 0, channels);
			// 4 bytes header, 1 byte command, 1 byte stream
			bw.Write(new byte[] { 0xde, 0xad, 0xbe, 0xef, 0x02, 0x00 });
			bw.Write((UInt16)channels);
			bw.Write(chdata);
		}

		// Close files
		codec.decode_close(data);
		bw.Flush();
		bw.Close();
		return 0;
	}
}
