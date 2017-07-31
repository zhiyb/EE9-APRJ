using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Xml;
using System.Xml.Serialization;

public class codec
{
	// Basics
	[DllImport("codec.dll")]
	public static extern void init();

	[DllImport("codec.dll")]
	public static extern int version();

	[DllImport("codec.dll", CharSet = CharSet.Ansi)]
	public static extern IntPtr find_codec(string codec_name);

	// Video encoder
	[DllImport("codec.dll", CharSet = CharSet.Ansi)]
	public static extern IntPtr encode_open_output(string file, string comment);

	[DllImport("codec.dll")]
	public static extern int encode_add_audio_stream_copy(IntPtr data, IntPtr dec_ac);

	[DllImport("codec.dll", CharSet = CharSet.Ansi)]
	public static extern int encode_add_video_stream(IntPtr data, IntPtr vcodec,
			string pix_fmt_name, int resolution, int channels);

	[DllImport("codec.dll", CharSet = CharSet.Ansi)]
	public static extern IntPtr encode_write_header(IntPtr data, string file);

	[DllImport("codec.dll")]
	public static extern int encode_write_packet_or_frame(IntPtr data,
			IntPtr pkt, IntPtr frame);

	[DllImport("codec.dll")]
	public static extern void encode_close(IntPtr data);

	// Video decoder
	[DllImport("codec.dll")]
	public static extern IntPtr decode_open_input(string file, ref IntPtr comment,
			out int gota, out int gotv);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_context(IntPtr data, int video);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_read_packet(IntPtr data,
			out int got, out int video);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_video_frame(IntPtr data, IntPtr pkt);

	[DllImport("codec.dll")]
	public static extern void decode_free_packet(IntPtr pkt);

	[DllImport("codec.dll")]
	public static extern void decode_close(IntPtr data);

	// Channel data
	[DllImport("codec.dll")]
	public static extern IntPtr encode_channels(IntPtr data,
			byte[] frame, int channels);

	[DllImport("codec.dll")]
	public static extern IntPtr decode_channels(IntPtr data, IntPtr frame,
			int channels);
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
			return Encode(input, output);
		else
			return Decode(input, output);
	}

	private static string FindMedia(string path)
	{
		if (path == null)
			return null;
		//Console.WriteLine("Trying media file path " + path);
		if (new FileInfo(path).Exists)
			return path;
		path = Path.GetFileName(path);
		//Console.WriteLine("Trying media file path " + path);
		if (new FileInfo(path).Exists)
			return path;
		return null;
	}

	private static string XmlCompact(string content)
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
		try {
			UInt16 channels = ReadHeader(br);
			return br.ReadBytes(channels);
		} catch (Exception /*e*/) {
			return null;
		}
	}

	private static int Encode(string input, string output)
	{
		// Read XML file as string for metadata
		var fileReader = new StreamReader(input);
		var strXml = XmlCompact(fileReader.ReadToEnd());
		fileReader.Close();

		// Deserialise XML controller configuration
		var xmlReader = new StringReader(strXml);
		var serializer = new XmlSerializer(typeof(Export));
		Export export = (Export)serializer.Deserialize(xmlReader);
		xmlReader.Close();

		// Find media file
		var mediaFile = export.Media.FirstOrDefault();
		var media = FindMedia(mediaFile);
		if (media == null && mediaFile != null)
			Console.WriteLine("Media " + mediaFile + " not found");

		// Find sequence file
		var outFile = export.OutFile;
		if (!new FileInfo(outFile).Exists)
			outFile = Path.Combine(Path.GetDirectoryName(input),
					Path.GetFileName(outFile));
		if (!new FileInfo(outFile).Exists) {
			Console.WriteLine("Sequence file " + export.OutFile + " not found");
			return 1;
		}

		// Extract channel information
		var con = export.Network.Last();
		int channels = con.StartChan + con.Channels - 1;
		Console.WriteLine("Encoding " + channels + " channels to " + output);

		// Open video file for encoding output
		IntPtr data = codec.encode_open_output(output, strXml);
		if (data == IntPtr.Zero)
			return 1;

		// Find video encoding codec
		IntPtr vcodec = codec.find_codec("libx264rgb");
		if (vcodec == IntPtr.Zero)
			return 1;

		// Add video stream
		int vstream = codec.encode_add_video_stream(data, vcodec,
				"rgb24", (int)export.Resolution, channels);
		if (vstream < 0) {
			codec.encode_close(data);
			return 1;
		}

		// Open media file for audio muxing
		IntPtr ac = IntPtr.Zero;
		IntPtr mdata = IntPtr.Zero;
		if (media != null) {
			IntPtr comment = IntPtr.Zero;
			int gota, gotv;
			mdata = codec.decode_open_input(media, ref comment, out gota, out gotv);
			if (gota == 0)
				codec.decode_close(mdata);
			else if (mdata != IntPtr.Zero)
				ac = codec.decode_context(mdata, 0);
		}

		// Add audio stream
		int astream = codec.encode_add_audio_stream_copy(data, ac);
		if (astream < 0) {
			codec.decode_close(mdata);
			mdata = IntPtr.Zero;
		}

		// Write file header and start encoding
		if (codec.encode_write_header(data, output) == IntPtr.Zero) {
			codec.decode_close(mdata);
			codec.encode_close(data);
			return 1;
		}

		// Open sequence dump file
		BinaryReader br = new BinaryReader(File.OpenRead(outFile));
		if (br == null) {
			codec.decode_close(mdata);
			codec.encode_close(data);
			return 1;
		}

		// Frame processing
		IntPtr apkt = IntPtr.Zero, vframe = IntPtr.Zero;
		for (;;) {
			// Read audio packet from media file
			if (mdata != IntPtr.Zero && apkt == IntPtr.Zero) {
				for (;;) {
					IntPtr pkt;
					int got, video;
					pkt = codec.decode_read_packet(mdata, out got, out video);
					if (got == 0)
						break;
					else if (video == 0) {
						apkt = pkt;
						break;
					} else {
						codec.decode_free_packet(pkt);
						continue;
					}
				}
			}
			// Read rendering frame from sequence dump
			if (vframe == IntPtr.Zero) {
				byte[] chdata = ReadFrame(br);
				if (chdata != null)
					vframe = codec.encode_channels(data, chdata, channels);
			}
			// Write audio packet or video frame
			int ret = codec.encode_write_packet_or_frame(data, apkt, vframe);
			if (ret == 0)
				apkt = IntPtr.Zero;
			else if (ret == 1)
				vframe = IntPtr.Zero;
			else
				break;
		}

		// Close files
		codec.decode_close(mdata);
		codec.encode_close(data);
		br.Close();
		return 0;
	}

	private static int Decode(string input, string output)
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
			IntPtr pkt = codec.decode_read_packet(data,
					out got, out video);
			if (got == 0)
				break;
			if (video == 0) {
				codec.decode_free_packet(pkt);
				continue;
			}
			IntPtr frame = codec.decode_video_frame(data, pkt);
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
