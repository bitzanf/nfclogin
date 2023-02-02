using Android.App;
using Android.Content;
//using Android.OS;
using Android.Runtime;
using Android.Views;
using Android.Widget;
using Java.IO;
using Java.Util;
using PemUtils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using Xamarin.Forms;

[assembly: Dependency(typeof(NFCLoginApp.Droid.CryptoServices))]
namespace NFCLoginApp.Droid
{
	public class CryptoServices : ICryptoServices
	{
		public CryptoServices()
		{
			PrepareKeyPair();
			generateFingerprint();
			csp.ImportParameters(keypair.ExportParameters(true));
		}

		public RSA KeyFromPEM(string pem)
		{
			using var stream = new MemoryStream(Encoding.UTF8.GetBytes(pem));
			using var reader = new PemReader(stream);
			return RSA.Create(reader.ReadRsaKey());
		}

		public string KeyToPEM(RSA key, bool isPrivate)
		{
			using var stream = new MemoryStream();
			using var writer = new PemWriter(stream);
			if (isPrivate) writer.WritePrivateKey(key);
			else writer.WritePublicKey(key);

			return stream.ToString();
		}

		public byte[] Transcode(RSA devPK, byte[] data)
		{
			// OpenSSL má Pkcs1 defaultně
			byte[] decoded = csp.Decrypt(data, RSAEncryptionPadding.Pkcs1);

			var cspEncode = new RSACryptoServiceProvider();
			cspEncode.ImportParameters(devPK.ExportParameters(false));
			return cspEncode.Encrypt(decoded, RSAEncryptionPadding.Pkcs1);
		}
		
		void PrepareKeyPair()
		{
			string path = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
			try
			{
				using var stream = new FileStream(Path.Combine(path, "private.pem"), FileMode.Open);
				using var reader = new PemReader(stream);
				keypair = RSA.Create(reader.ReadRsaKey());
			} catch (System.IO.FileNotFoundException)
			{
				keypair = RSA.Create(2048);
			}
			PublicKeyPEM = KeyToPEM(keypair, false);
			generateFingerprint();
		}

		void generateFingerprint()
		{
			var data = keypair.ExportParameters(false);

			SHA1 sha1 = SHA1.Create();
			byte[] hash = sha1.ComputeHash(data.Modulus);

			//brutálně nechutný, ale jiná možnost tady moc není...
			//looking at you Convert.ToHexString()...
			var sb = new StringBuilder();
			hash.Select(b => string.Format("{0:x}", b)).ToList().ForEach(s => sb.Append(s));
			Fingerprint = sb.ToString();
		}

		public string PublicKeyPEM { get; private set; }
		public string Fingerprint { get; private set; }

		RSA keypair;
		RSACryptoServiceProvider csp = new RSACryptoServiceProvider();
	}
}