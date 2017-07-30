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
	public static extern void init();

	[DllImport("codec.dll")]
	public static extern int version();

	[DllImport("codec.dll", CharSet = CharSet.Ansi)]
	public static extern IntPtr find_codec(string codec_name);

	[DllImport("codec.dll", CharSet = CharSet.Ansi)]
	public static extern IntPtr encode_open_output(string file,
			IntPtr acodec, IntPtr vcodec, string pix_fmt_name,
			int resolution, int channels, string comment);

	[DllImport("codec.dll")]
	public static extern int encode_write_video_frame(IntPtr data,
			byte[] frame, int channels);

	[DllImport("codec.dll")]
	public static extern void encode_close(IntPtr data);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_open_input(string file, ref IntPtr comment,
			out int gota, out int gotv);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_read_frame(IntPtr data,
			out int got, out int video);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_channels(IntPtr data, IntPtr frame,
			int channels);

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
		Console.WriteLine((enc ? "Encoding " : "Decoding ") +
				input + " to " + output);

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

	private static string xmlCompact(string content)
	{
		XmlDocument doc = new XmlDocument();
		doc.LoadXml(content);
		var strWriter = new StringWriter();
		using (XmlTextWriter writer = new XmlTextWriter(strWriter)) {
			writer.Formatting = Formatting.None;
			doc.Save(writer);
		}
		return strWriter.ToString();
	}

	private static int encode(string input, string output)
	{
		// Read XML file as string for metadata
		var fileReader = new StreamReader(input);
		var strXml = xmlCompact(fileReader.ReadToEnd());
		fileReader.Close();

		// Deserialise XML controller configuration
		var xmlReader = new StringReader(strXml);
		var serializer = new XmlSerializer(typeof(Export));
		Export export = (Export)serializer.Deserialize(xmlReader);
		xmlReader.Close();

		var con = export.Network.Last();
		int channels = con.StartChan + con.Channels - 1;
		Console.WriteLine("Encoding " + channels + " channels to " + output);

		// Find encoding codecs
		IntPtr acodec = IntPtr.Zero;
		IntPtr vcodec = codec.find_codec("libx264rgb");
		if (vcodec == IntPtr.Zero)
			return 1;

		// Open video file for encoding output
		IntPtr data = codec.encode_open_output(output, acodec, vcodec,
				"rgb24", (int)export.Resolution, channels, strXml);
		if (data == IntPtr.Zero)
			return 1;

		// Open raw file for encoding input
		BinaryReader br = new BinaryReader(File.OpenRead(export.OutFile));
		try {
			for (;;) {
				byte[] frame = ReadFrame(br);
				if (codec.encode_write_video_frame(data,
							frame, channels) == 0)
					break;
			}
		} catch (Exception /*e*/) {
			//Console.WriteLine("Exception: " + e.ToString());
		}

		// Close files
		codec.encode_close(data);
		br.Close();
		return 0;
	}

	private static int decode(string input, string output)
	{
		// Open video file for decoding input
		IntPtr cmtp = new IntPtr();
		int gota, gotv;
		IntPtr data = codec.decode_open_input(input, ref cmtp,
				out gota, out gotv);
		if (data == IntPtr.Zero)
			return 1;
		if (gotv == 0) {
			codec.decode_close(data);
			return 1;
		}
		string strXml = Marshal.PtrToStringAnsi(cmtp);

		// Deserialise XML controller configuration
		var xmlReader = new StringReader(strXml);
		var serializer = new XmlSerializer(typeof(Export));
		Export export = (Export)serializer.Deserialize(xmlReader);
		xmlReader.Close();

		var con = export.Network.Last();
		int channels = con.StartChan + con.Channels - 1;
		Console.WriteLine(channels + " channels available from " + input);
		byte[] chdata = new byte[channels];

		// Read video frames and write to binary file
		BinaryWriter bw = new BinaryWriter(File.Create(output));
		for (;;) {
			int got = 0, video = 0;
			IntPtr frame = codec.decode_read_frame(data,
					out got, out video);
			if (got == 0)
				break;
			if (video == 0)
				continue;
			if (frame != IntPtr.Zero) {
				IntPtr a = codec.decode_channels(data, frame, channels);
				Marshal.Copy(a, chdata, 0, channels);
			}
			// 4 bytes header, 1 byte command, 1 byte stream
			bw.Write(new byte[] { 0xde, 0xad, 0xbe, 0xef, 0x02, 0x00 });
			bw.Write((UInt16)channels);
			bw.Write(chdata);
		}

		// Write network XML
		export.OutFile = output;
		string xmlOutput = Path.Combine(Path.GetDirectoryName(output),
			Path.GetFileNameWithoutExtension(output) +
			"_Network.xml");
		XmlWriterSettings settings = new XmlWriterSettings();
		settings.Indent = true;
		settings.IndentChars = "\t";
		serializer.Serialize(XmlWriter.Create(xmlOutput, settings), export);

		// Close files
		codec.decode_close(data);
		bw.Flush();
		bw.Close();
		return 0;
	}
}
