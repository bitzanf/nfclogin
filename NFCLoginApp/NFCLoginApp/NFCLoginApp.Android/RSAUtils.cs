using Android.App;
using Android.Content;
using Android.OS;
using Android.Runtime;
using Android.Views;
using Android.Widget;
using PemUtils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

// https://itecnote.com/tecnote/c-export-private-public-rsa-key-from-rsacryptoserviceprovider-to-pem-string/
namespace NFCLoginApp.Droid
{
	public class RSAUtils
	{
		public static string DER2PEM(byte[] der)
		{
			RSA key = RSA.Create();
			key.ImportRSAPublicKey(der, out _);
			using var stream = new MemoryStream();
			using var writer = new PemWriter(stream);
			writer.WritePublicKey(key);
			return stream.ToString();
		}

		public static byte[] ExportPublicKey(RSAParameters parameters)
		{
			using (var stream = new MemoryStream())
			{
				var writer = new BinaryWriter(stream);
				writer.Write((byte)0x30); // SEQUENCE
				using (var innerStream = new MemoryStream())
				{
					var innerWriter = new BinaryWriter(innerStream);
					EncodeIntegerBigEndian(innerWriter, parameters.Modulus);
					EncodeIntegerBigEndian(innerWriter, parameters.Exponent);
					var length = (int)innerStream.Length;
					EncodeLength(writer, length);
					writer.Write(innerStream.GetBuffer(), 0, length);
				}

				return stream.GetBuffer()[..(Index)stream.Length];
			}
		}

		private static void EncodeLength(BinaryWriter stream, int length)
		{
			if (length < 0) throw new ArgumentOutOfRangeException("length", "Length must be non-negative");
			if (length < 0x80)
			{
				// Short form
				stream.Write((byte)length);
			}
			else
			{
				// Long form
				var temp = length;
				var bytesRequired = 0;
				while (temp > 0)
				{
					temp >>= 8;
					bytesRequired++;
				}
				stream.Write((byte)(bytesRequired | 0x80));
				for (var i = bytesRequired - 1; i >= 0; i--)
				{
					stream.Write((byte)(length >> (8 * i) & 0xff));
				}
			}
		}

		private static void EncodeIntegerBigEndian(BinaryWriter stream, byte[] value, bool forceUnsigned = true)
		{
			stream.Write((byte)0x02); // INTEGER
			var prefixZeros = 0;
			for (var i = 0; i < value.Length; i++)
			{
				if (value[i] != 0) break;
				prefixZeros++;
			}
			if (value.Length - prefixZeros == 0)
			{
				EncodeLength(stream, 1);
				stream.Write((byte)0);
			}
			else
			{
				if (forceUnsigned && value[prefixZeros] > 0x7f)
				{
					// Add a prefix zero to force unsigned if the MSB is 1
					EncodeLength(stream, value.Length - prefixZeros + 1);
					stream.Write((byte)0);
				}
				else
				{
					EncodeLength(stream, value.Length - prefixZeros);
				}
				for (var i = prefixZeros; i < value.Length; i++)
				{
					stream.Write(value[i]);
				}
			}
		}
	}
}