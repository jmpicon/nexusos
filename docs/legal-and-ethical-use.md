# NexusOS — Legal and Ethical Use Policy

## Purpose

NexusOS is a professional cybersecurity Linux distribution designed exclusively for:

- **Authorised security research** conducted within agreed scope
- **Digital forensics and incident response** on systems you own or have explicit
  written authorisation to examine
- **Defensive security operations** including hardening, monitoring, and auditing
- **Security education and training** in controlled environments
- **Penetration testing** under written authorisation (scope agreement)
- **Malware analysis** in isolated, controlled laboratory environments

---

## What NexusOS Includes and Why

| Tool category         | Purpose in NexusOS              | Justification                   |
|----------------------|----------------------------------|---------------------------------|
| Network scanners     | Authorised asset discovery       | Essential for security audits   |
| Packet capture       | Traffic analysis and monitoring  | Standard DFIR and SOC work      |
| Forensic tools       | Evidence acquisition and analysis| Core DFIR discipline            |
| Memory analysis      | Incident response               | Standard IR procedure           |
| Disassemblers        | Binary analysis, reverse eng    | Malware research, vuln research |
| Debuggers            | Software analysis               | Development and research        |
| Vulnerability tools  | Authorised scanning             | Security assessments            |
| Container scanners   | Cloud security posture          | DevSecOps and cloud auditing    |
| Hardening tools      | System and configuration review | Defensive security              |
| YARA                 | Malware detection signatures    | Threat detection                |

---

## What NexusOS Does NOT Include

NexusOS intentionally excludes tools and capabilities designed primarily for:

- **Ransomware or destructive malware** of any kind
- **Credential dumping** from live production systems (beyond authorised IR)
- **Remote access trojans (RATs)** or implants for unauthorised access
- **Botnets or DDoS infrastructure**
- **Automated exploitation frameworks** against third-party systems
- **Persistence mechanisms** for unauthorised access
- **Anti-forensic or detection evasion** tools for malicious actors
- **Social engineering automation** targeting real individuals
- **Supply chain attack tooling**

---

## User Responsibilities

By using NexusOS, you agree that:

1. **You have legal authority** over the systems and networks you test, audit,
   or analyse. This includes written authorisation from asset owners for any
   penetration testing or scanning activities.

2. **You comply with all applicable laws** in your jurisdiction, including
   computer crime laws, data protection laws (e.g., GDPR), and export control
   regulations.

3. **You use NexusOS in isolated environments** when analysing potentially
   malicious software, to prevent accidental infection or data exposure.

4. **You handle evidence responsibly** and in accordance with chain-of-custody
   requirements when performing DFIR work.

5. **You do not use NexusOS to harm, disrupt, or access systems or data
   without authorisation.**

---

## Legal Framework (Non-exhaustive)

The following laws are relevant to the use of security tools. This is not
legal advice — consult a qualified lawyer in your jurisdiction:

- **EU:** Computer Misuse laws vary by member state; GDPR applies to data handling
- **Spain:** Ley Orgánica 10/1995 (Código Penal, arts. 197-200); RGPD
- **UK:** Computer Misuse Act 1990; Investigatory Powers Act 2016
- **USA:** Computer Fraud and Abuse Act (CFAA); Electronic Communications Privacy Act
- **International:** Budapest Convention on Cybercrime

---

## Reporting Security Issues

If you discover a security vulnerability in NexusOS itself or in a tool
included in the distribution, please disclose it responsibly:
- Open an issue on the project repository
- Label it `security` and provide sufficient detail for reproduction
- Allow 90 days for a fix before public disclosure

---

## Disclaimer

NexusOS is provided "as is" without warranty of any kind. The NexusOS project
and its contributors are not responsible for any damage, legal consequences,
or data loss resulting from the use or misuse of this distribution.

Use of NexusOS for any activity that violates applicable law is strictly
prohibited and solely the responsibility of the user.
