using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.IO;
using System.Media;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

// Portable desktop companion. The original source pixels are clipped by a
// window region; the character is neither redrawn nor generated.
internal static class Program
{
    [DllImport("user32.dll")] private static extern bool SetProcessDPIAware();
    [STAThread]
    private static void Main(string[] args)
    {
        try { SetProcessDPIAware(); } catch { }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        if (args.Length == 2 && args[0] == "--render-preview")
        {
            using (var pet = new PetForm(false)) pet.RenderPreview(args[1]);
            return;
        }
        bool created;
        using (var mutex = new Mutex(true, "Local\\BADOINGDesktopPet", out created))
        {
            if (!created) return;
            try { Application.Run(new PetForm(true)); }
            catch (Exception error)
            {
                MessageBox.Show("BADOING could not start.\n\n" + error.Message,
                    "BADOING", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            finally { mutex.ReleaseMutex(); }
        }
    }
}

internal sealed class PetForm : Form
{
    private const int SourceWidth = 426, SourceHeight = 240;
    // No transparent margin below the flat log base: the artwork touches the
    // taskbar when the window bottom matches WorkingArea.Bottom.
    private static readonly Rectangle Crop = new Rectangle(30, 106, 318, 92);
    private static readonly int[] Delays = {70,70,60,70,70,60,70,70,60,70};
    // Outline follows the source log and creature. Sky/earth outside it are not
    // part of the window, so clicks there go through to the desktop underneath.
    private static readonly PointF[] Outline = {
        new PointF(33,174), new PointF(41,172), new PointF(48,172),
        new PointF(57,174), new PointF(69,174), new PointF(80,171),
        new PointF(91,169), new PointF(112,170), new PointF(131,170),
        new PointF(135,167), new PointF(144,164), new PointF(153,159),
        new PointF(149,154), new PointF(139,147), new PointF(128,140),
        new PointF(116,135), new PointF(100,131),
        new PointF(99,128), new PointF(94,124), new PointF(107,127),
        new PointF(122,128), new PointF(140,138), new PointF(157,148),
        new PointF(165,155), new PointF(174,146), new PointF(179,141),
        new PointF(176,133), new PointF(175,126), new PointF(178,118),
        new PointF(186,112), new PointF(194,107), new PointF(201,108),
        new PointF(210,111), new PointF(219,115), new PointF(229,122),
        new PointF(237,130), new PointF(249,135), new PointF(261,142),
        new PointF(270,150), new PointF(277,158), new PointF(278,166),
        new PointF(294,171), new PointF(309,176), new PointF(324,184),
        new PointF(339,192), new PointF(348,198),
        new PointF(62,198), new PointF(63,194), new PointF(61,190),
        new PointF(54,188), new PointF(59,185), new PointF(60,183),
        new PointF(57,180), new PointF(51,177), new PointF(43,175)
    };
    private readonly Bitmap[] artwork = new Bitmap[10];
    private readonly Region[] outlines = new Region[10];
    private readonly Stopwatch clock = new Stopwatch();
    private readonly System.Windows.Forms.Timer animation = new System.Windows.Forms.Timer();
    private readonly System.Windows.Forms.Timer screenWatch = new System.Windows.Forms.Timer();
    private readonly SoundPlayer sound;
    private readonly Stream soundStream;
    private readonly NotifyIcon tray;
    private readonly ToolStripMenuItem countItem = new ToolStripMenuItem();
    private readonly ToolStripMenuItem muteItem = new ToolStripMenuItem("Mute");
    private readonly ToolTip tooltip = new ToolTip();
    private readonly string settingsPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "BADOING.settings");
    private readonly bool persist;
    private long count;
    private int frame;
    private int petWidth = 355;
    private bool muted, pointerDown, dragging, docked = true;
    private Point pointerOrigin, windowOrigin;
    private Rectangle lastWorkArea;

    internal PetForm(bool saveSettings)
    {
        persist = saveSettings;
        Text = "BADOING desktop pet";
        Name = "BADOINGPet";
        AccessibleName = "BADOING desktop pet";
        AccessibleDescription = "Click for badoing. Drag to move. Right-click for mute, size, and quit.";
        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = true;
        TopMost = true;
        StartPosition = FormStartPosition.Manual;
        BackColor = Color.Magenta;
        TransparencyKey = Color.Magenta;
        DoubleBuffered = true;
        Cursor = Cursors.Hand;
        AutoScaleMode = AutoScaleMode.None;
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint |
                 ControlStyles.OptimizedDoubleBuffer, true);

        using (var resource = Assembly.GetExecutingAssembly().GetManifestResourceStream("frames.png"))
        using (var sheet = new Bitmap(resource))
        {
            if (sheet.Width != SourceWidth * 10 || sheet.Height != SourceHeight)
                throw new InvalidDataException("The animation resource is invalid.");
            for (int i = 0; i < 10; i++)
            {
                artwork[i] = sheet.Clone(new Rectangle(i*SourceWidth+Crop.X, Crop.Y, Crop.Width, Crop.Height),
                    System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            }
        }
        soundStream = Assembly.GetExecutingAssembly().GetManifestResourceStream("badoing.wav");
        sound = new SoundPlayer(soundStream);
        sound.Load();
        if (persist) LoadSettings();
        BuildRegions();
        ApplySize();
        if (Location == Point.Empty || !IsOnScreen()) SitOnTaskbar(Screen.PrimaryScreen);
        else if (docked) SitOnTaskbar(Screen.FromControl(this));
        else { ClampToScreen(); lastWorkArea = Screen.FromControl(this).WorkingArea; }

        var menu = new ContextMenuStrip();
        countItem.Enabled = false;
        menu.Items.Add(countItem);
        menu.Items.Add(new ToolStripSeparator());
        muteItem.Checked = muted;
        muteItem.Click += delegate { muted = !muted; muteItem.Checked = muted; if (muted) sound.Stop(); SaveSettings(); };
        menu.Items.Add(muteItem);
        var sizes = new ToolStripMenuItem("Size");
        AddSize(sizes, "Small", 260);
        AddSize(sizes, "Normal", 355);
        AddSize(sizes, "Large", 470);
        menu.Items.Add(sizes);
        menu.Items.Add("Sit on taskbar", null, delegate { SitOnTaskbar(Screen.FromControl(this)); SaveSettings(); });
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Quit", null, delegate { Close(); });
        ContextMenuStrip = menu;
        tray = new NotifyIcon { Icon = SystemIcons.Application, Text = "BADOING", ContextMenuStrip = menu, Visible = persist };
        tray.DoubleClick += delegate { SitOnTaskbar(Screen.PrimaryScreen); Show(); };
        UpdateCountLabel();

        animation.Interval = 15;
        animation.Tick += delegate { AdvanceFrame(); };
        screenWatch.Interval = 1500;
        screenWatch.Tick += delegate {
            Rectangle area = Screen.FromControl(this).WorkingArea;
            if (area != lastWorkArea)
            {
                if (docked) SitOnTaskbar(Screen.FromControl(this)); else ClampToScreen();
                lastWorkArea = area;
            }
        };
        if (persist) screenWatch.Start();
    }

    protected override bool ShowWithoutActivation { get { return true; } }
    protected override void WndProc(ref Message m)
    {
        if (m.Msg == 0x21) { m.Result = new IntPtr(3); return; } // MA_NOACTIVATE
        base.WndProc(ref m);
    }

    private void BuildRegions()
    {
        using (var path = new GraphicsPath())
        {
            var points = new PointF[Outline.Length];
            for (int i = 0; i < points.Length; i++)
                points[i] = new PointF(Outline[i].X-Crop.X, Outline[i].Y-Crop.Y);
            path.AddPolygon(points);
            // A tight geometric outline preserves the source's dithered pixels
            // without the pinholes left by filtering individual sky colours.
            using (var silhouette = new Region(path))
            {
                for (int n = 0; n < artwork.Length; n++)
                    outlines[n] = silhouette.Clone();
            }
        }
    }

    private void ApplySize()
    {
        ClientSize = new Size(petWidth, (int)Math.Round(Crop.Height * petWidth / (double)Crop.Width));
        ApplyRegion();
        Invalidate();
    }
    private void ApplyRegion()
    {
        var region = outlines[frame].Clone();
        using (var transform = new Matrix())
        {
            transform.Scale(ClientSize.Width/(float)Crop.Width, ClientSize.Height/(float)Crop.Height);
            region.Transform(transform);
        }
        var old = Region; Region = region; if (old != null) old.Dispose();
    }
    private void AddSize(ToolStripMenuItem parent, string label, int width)
    {
        parent.DropDownItems.Add(label, null, delegate {
            petWidth = width; ApplySize();
            if (docked) SitOnTaskbar(Screen.FromControl(this)); else ClampToScreen();
            SaveSettings();
        });
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.InterpolationMode = InterpolationMode.NearestNeighbor;
        e.Graphics.PixelOffsetMode = PixelOffsetMode.Half;
        e.Graphics.DrawImage(artwork[frame], ClientRectangle, 0, 0, Crop.Width, Crop.Height, GraphicsUnit.Pixel);
    }
    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);
        if (e.Button != MouseButtons.Left) return;
        pointerDown = true; dragging = false; pointerOrigin = MousePosition; windowOrigin = Location; Capture = true;
    }
    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);
        if (!pointerDown) return;
        Point delta = new Point(MousePosition.X-pointerOrigin.X, MousePosition.Y-pointerOrigin.Y);
        if (Math.Abs(delta.X)>5 || Math.Abs(delta.Y)>5) dragging = true;
        if (dragging) { Location = new Point(windowOrigin.X+delta.X, windowOrigin.Y+delta.Y); docked=false; }
    }
    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        if (e.Button != MouseButtons.Left || !pointerDown) return;
        pointerDown=false; Capture=false;
        if (dragging)
        {
            ClampToScreen();
            Screen screen = Screen.FromControl(this);
            // A small drop distance is enough to settle the flat base onto it.
            if (Math.Abs(Bottom - screen.WorkingArea.Bottom) <= 24) SitOnTaskbar(screen);
            SaveSettings();
        }
        else Badoing();
        dragging=false;
    }
    private void Badoing()
    {
        if (count < long.MaxValue) count++;
        UpdateCountLabel(); SaveSettings();
        frame=0; ApplyRegion(); Invalidate(); clock.Restart(); animation.Start();
        if (!muted) { try { sound.Play(); } catch { } }
    }
    private void AdvanceFrame()
    {
        long elapsed = clock.ElapsedMilliseconds;
        if (elapsed >= 670) { animation.Stop(); clock.Stop(); SetFrame(0); return; }
        int selected=0, boundary=Delays[0];
        while (elapsed >= boundary && selected < Delays.Length-1) boundary += Delays[++selected];
        SetFrame(selected);
    }
    private void SetFrame(int selected)
    {
        if (frame == selected) return;
        frame = selected; ApplyRegion(); Invalidate();
    }
    private void UpdateCountLabel()
    {
        countItem.Text = count.ToString("N0", CultureInfo.InvariantCulture) + " badoings";
        tooltip.SetToolTip(this, "badoing · " + count.ToString("N0", CultureInfo.InvariantCulture));
    }
    private bool IsOnScreen()
    {
        foreach (var screen in Screen.AllScreens)
            if (screen.WorkingArea.IntersectsWith(Bounds)) return true;
        return false;
    }
    private void ClampToScreen()
    {
        Rectangle area = Screen.FromControl(this).WorkingArea;
        Location = new Point(Math.Max(area.Left, Math.Min(Left, area.Right-Width)),
                             Math.Max(area.Top, Math.Min(Top, area.Bottom-Height)));
    }
    private void SitOnTaskbar(Screen screen)
    {
        Rectangle area = screen.WorkingArea;
        int x = Location == Point.Empty ? area.Right-Width-40 : Left;
        Location = new Point(Math.Max(area.Left, Math.Min(x, area.Right-Width)), area.Bottom-Height);
        lastWorkArea=area; docked=true;
    }
    private void LoadSettings()
    {
        try
        {
            string[] lines=File.ReadAllLines(settingsPath);
            long saved; int x,y,width;
            if (lines.Length>=6 && long.TryParse(lines[0],out saved) && saved>=0 &&
                int.TryParse(lines[1],out x) && int.TryParse(lines[2],out y) && int.TryParse(lines[4],out width))
            {
                count=saved; Location=new Point(x,y); muted=lines[3]=="1";
                petWidth=width==260||width==355||width==470 ? width : 355; docked=lines[5]=="1";
            }
        }
        catch { }
    }
    private void SaveSettings()
    {
        if (!persist) return;
        try {
            File.WriteAllLines(settingsPath, new[] { count.ToString(CultureInfo.InvariantCulture),
                Left.ToString(CultureInfo.InvariantCulture), Top.ToString(CultureInfo.InvariantCulture),
                muted?"1":"0", petWidth.ToString(CultureInfo.InvariantCulture), docked?"1":"0" });
        } catch { }
    }
    internal void RenderPreview(string path)
    {
        using (var output = new Bitmap(Crop.Width, Crop.Height))
        using (var graphics = Graphics.FromImage(output))
        {
            graphics.Clear(Color.Transparent); graphics.SetClip(outlines[0], CombineMode.Replace);
            graphics.DrawImageUnscaled(artwork[0],0,0);
            output.Save(path, System.Drawing.Imaging.ImageFormat.Png);
        }
    }
    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        SaveSettings();
        base.OnFormClosed(e);
    }
    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            animation.Dispose(); screenWatch.Dispose(); tooltip.Dispose();
            if (tray!=null) { tray.Visible=false; tray.Dispose(); }
            if (sound!=null) sound.Dispose(); if (soundStream!=null) soundStream.Dispose();
            foreach(var item in artwork) if(item!=null) item.Dispose();
            foreach(var item in outlines) if(item!=null) item.Dispose();
        }
        base.Dispose(disposing);
    }
}
