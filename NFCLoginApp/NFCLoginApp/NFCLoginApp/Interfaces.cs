using PemUtils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace NFCLoginApp
{
	public interface ICryptoServices
	{
		RSA KeyFromPEM(string pem);
		string KeyToPEM(RSA key, bool isPrivate);
		
		byte[] Transcode(RSA devPK, byte[] data);

		string PublicKeyPEM { get; }
		string Fingerprint { get; }
	}

	public interface IBioAuth
	{
		void RequestFPAuth(string title, string desc, Action success, Action nosuccess, Action<int, string> error);
		bool CheckFPHW(out string reason);
	}
}
