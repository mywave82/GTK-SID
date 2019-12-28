#include <cairo.h>
#include <stdint.h>
#include <gtk/gtk.h>

#include "pulse.h"
#include "sid-instance.h"

const int voice_index[3] = {0, 1, 2};

GtkWidget *FREQ[3];
GtkWidget *PW[3];

GtkWidget *SYNC[3];
GtkWidget *GATE[3];
GtkWidget *TEST[3];
GtkWidget *RING[3];

GtkWidget *Triangle[3];
GtkWidget *Saw[3];
GtkWidget *Pulse[3];
GtkWidget *Noise[3];

GtkWidget *Attack[3];
GtkWidget *Decay[3];
GtkWidget *Sustain[3];
GtkWidget *Release[3];

GtkWidget *FC;
GtkWidget *RES;
GtkWidget *LP, *BP, *HP;
GtkWidget *_3_OFF;
GtkWidget *FILT[3];
GtkWidget *VOL;

GtkWidget *graph;

GtkListStore *AttackModel;
GtkListStore *SustainModel;
GtkListStore *DecayReleaseModel;

#define SAMPLES_N 512
int16_t samples[SAMPLES_N];

static void write_reg(uint8_t reg, uint8_t value)
{
	fprintf (stderr, "[%02x] = 0x%02x\n", reg, value);
	sid_instance_write (reg, value);
}

static void freq_value_changed (GtkSpinButton *spin_button,
                                gpointer       user_data)
{
	int *voice = user_data;
	int value;

	value = gtk_spin_button_get_value_as_int (spin_button);

	write_reg(*voice * 7 + 0, value & 0xff);
	write_reg(*voice * 7 + 1, value >> 8);
}

static void pw_value_changed (GtkSpinButton *spin_button,
                                gpointer       user_data)
{
	int *voice = user_data;
	int value;

	value = gtk_spin_button_get_value_as_int (spin_button);

	write_reg(*voice * 7 + 2, value & 0xff);
	write_reg(*voice * 7 + 3, value >> 8);
}

static void control_xx_changed (GtkToggleButton *toggle_button,
                                gpointer         user_data)
{
	int *voice = user_data;
	uint8_t value = 0;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(GATE[*voice])))     value |= 0x01;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(SYNC[*voice])))     value |= 0x02;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(RING[*voice])))     value |= 0x04;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(TEST[*voice])))     value |= 0x08;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(Triangle[*voice]))) value |= 0x10;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(Saw[*voice])))      value |= 0x20;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(Pulse[*voice])))    value |= 0x40;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(Noise[*voice])))    value |= 0x80;

	write_reg(*voice * 7 + 4, value);
}

static void AttackDecay_changed (GtkComboBox *combo,
                                 gpointer     user_data)
{
	int *voice = user_data;
	uint8_t value = 0;

	value |= gtk_combo_box_get_active (GTK_COMBO_BOX(Attack[*voice])) << 4;
	value |= gtk_combo_box_get_active (GTK_COMBO_BOX(Decay[*voice]));

	write_reg(*voice * 7 + 5, value);
}

static void SustainRelease_changed (GtkComboBox *combo,
                                 gpointer     user_data)
{
	int *voice = user_data;
	uint8_t value = 0;

	value |= gtk_combo_box_get_active (GTK_COMBO_BOX(Sustain[*voice])) << 4;
	value |= gtk_combo_box_get_active (GTK_COMBO_BOX(Release[*voice]));

	write_reg(*voice * 7 + 6, value);
}

static void fc_value_changed (GtkSpinButton *spin_button,
                              gpointer       user_data)
{
	int value;

	value = gtk_spin_button_get_value_as_int (spin_button);

	write_reg(0x15, value & 0x07);
	write_reg(0x16, value >> 3);
}

static void rc_filt_value_changed (gpointer sender,
                                   gpointer user_data)
{
	int value;

	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(RES))<<4;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(FILT[2]))) value |= 0x04;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(FILT[1]))) value |= 0x02;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(FILT[0]))) value |= 0x01;

	write_reg(0x17, value);
}

static void mode_vol_changed (gpointer sender,
                              gpointer user_data)
{
	uint8_t value = 0;

	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(VOL));
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(LP)))     value |= 0x10;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(BP)))     value |= 0x20;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(HP)))     value |= 0x40;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(_3_OFF))) value |= 0x80;

	write_reg(0x18, value);
}

static gboolean graph_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	int i;

	//gdouble dx, dy;
	guint width, height;
	//GdkRectangle da;            /* GtkDrawingArea size */
	GtkAllocation clip;

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	gtk_widget_get_clip (graph, &clip);

	/* Draw on a black background */
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_paint (cr);

	/* Change the transformation matrix */
	cairo_scale (cr, (gdouble)width/SAMPLES_N, ((gdouble)height)/-(256+5.0));
	cairo_translate (cr, 0, -128.0+2.5);

	/* Determine the data points to calculate (ie. those in the clipping zone */
	//cairo_device_to_user_distance (cr, &dx, &dy);
	//cairo_clip_extents (cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);

	//fprintf (stderr, "linewidth: %lf (%lf)\n", dx, dy);
	cairo_set_line_width (cr, 2.0);

	cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);

	cairo_move_to (cr, 0.0, (gdouble)samples[0]/SAMPLES_N);
	for (i=0; i < SAMPLES_N; i++)
	{
		cairo_line_to (cr, i, (gdouble)samples[i]/SAMPLES_N);
	}

	cairo_stroke (cr);

	return FALSE;
}

int inputlength = 0;
int16_t *input;

void mainwindow_getsound (void *data, size_t bytes)
{
	int i;
	int smallestvalue=65536;
	int smallestat = -1;
	int16_t *output = data;
	size_t size;

	int count;

	count = bytes/4;

	if (count > inputlength)
	{
		inputlength = count;
		input = realloc (input, inputlength*2);
	}

	sid_instance_clock (input, count);

	for (i=0; i< count; i++)
	{
		output[i<<1] = input[i];
		output[(i<<1)+1] = input[i];

		if (i + SAMPLES_N < (count))
		{
			if (input[i] < smallestvalue)
			{
				smallestvalue = input[i];
				smallestat = i;
			}
		}

	}

	if (smallestat >= 0)
	{
		memcpy (samples, input + smallestat, SAMPLES_N*sizeof(int16_t));

		gtk_widget_queue_draw (graph);
	}
}


static gboolean window_delete (GtkWidget *object,
                               gpointer   userpointer)
{
	sidpulse_done ();

	return FALSE;
}

static void
activate (GtkApplication* app,
          gpointer        user_data)
{
	GtkWidget *window, *thegrid;
	int i;

	sidpulse_init ();

	sid_instance_init ();

	AttackModel = gtk_list_store_new (1, G_TYPE_STRING);
	{
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "2ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "8ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "16ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "24ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "38ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "56ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "68ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "80ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "100ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "240ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "500ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "800ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "1s", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "3s", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "5s", -1);
		gtk_list_store_append (GTK_LIST_STORE (AttackModel), &iter); gtk_list_store_set (GTK_LIST_STORE (AttackModel), &iter, 0, "8s", -1);
	}

	SustainModel = gtk_list_store_new (1, G_TYPE_STRING);
	{
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "0 - no sound", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "1", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "2", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "3", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "4", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "5", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "6", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "7", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "8", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "9", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "10", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "11", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "12", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "13", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "14", -1);
		gtk_list_store_append (GTK_LIST_STORE (SustainModel), &iter); gtk_list_store_set (GTK_LIST_STORE (SustainModel), &iter, 0, "15 - full peak", -1);
	}

	DecayReleaseModel = gtk_list_store_new (1, G_TYPE_STRING);
	{
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "6ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "24ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "48ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "72ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "114ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "168ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "204ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "240ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "300ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "750ms", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "1.5s", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "2.5s", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "3s", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "9s", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "15s", -1);
		gtk_list_store_append (GTK_LIST_STORE (DecayReleaseModel), &iter); gtk_list_store_set (GTK_LIST_STORE (DecayReleaseModel), &iter, 0, "24s", -1);
	}

	window = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (window), "Window");
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
	g_signal_connect (window, "delete-event", G_CALLBACK(window_delete), 0);

	thegrid = gtk_grid_new ();
	gtk_container_add (GTK_CONTAINER (window), thegrid);

	for (i=0; i < 3; i++)
	{
		GtkWidget *temp, *frame, *subgrid, *subsubgrid;
		char text[64];
		snprintf (text, sizeof (text), "Voice %d (SYNC and MOD linked to voice %d)", i + 1, ((i + 2) % 3) + 1);

		frame = gtk_frame_new (text);
		gtk_widget_set_margin_top (frame, 10.0);
		gtk_widget_set_margin_bottom (frame, 10.0);
		gtk_widget_set_margin_start (frame, 10.0);
		gtk_widget_set_margin_end (frame, 10.0);

		gtk_grid_attach (GTK_GRID (thegrid), frame, 0, i, 1, 1);

		subgrid = gtk_grid_new ();
		gtk_container_add (GTK_CONTAINER (frame), subgrid);
		
		temp = gtk_label_new ("Frequency"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 0, 1, 1);
		FREQ[i] = gtk_spin_button_new_with_range (0x0000, 0xffff, 0x0001);
		gtk_grid_attach (GTK_GRID (subgrid), FREQ[i], 1, 0, 1, 1);
		g_signal_connect (FREQ[i], "value-changed", G_CALLBACK (freq_value_changed), (gpointer)(voice_index + i));

		temp = gtk_label_new ("PulseWidth"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 1, 1, 1);
		PW[i] = gtk_spin_button_new_with_range (0x000, 0xfff, 0x001);
		gtk_grid_attach (GTK_GRID (subgrid), PW[i], 1, 1, 1, 1);
		g_signal_connect (PW[i], "value-changed", G_CALLBACK (pw_value_changed), (gpointer)(voice_index + i));

		subsubgrid = gtk_grid_new ();
		gtk_grid_set_column_homogeneous (GTK_GRID (subsubgrid), TRUE);
		gtk_grid_attach (GTK_GRID (subgrid), subsubgrid, 0, 2, 2, 1);

		SYNC[i] = gtk_toggle_button_new_with_label ("Sync");
		gtk_grid_attach (GTK_GRID (subsubgrid), SYNC[i], 0, 1, 1, 1);
		g_signal_connect (SYNC[i], "toggled", G_CALLBACK (control_xx_changed), (gpointer)(voice_index + i));

		TEST[i] = gtk_toggle_button_new_with_label ("Test");
		gtk_grid_attach (GTK_GRID (subsubgrid), TEST[i], 1, 1, 1, 1);
		g_signal_connect (TEST[i], "toggled", G_CALLBACK (control_xx_changed), (gpointer)(voice_index + i));

		GATE[i] = gtk_toggle_button_new_with_label ("Gate");
		gtk_grid_attach (GTK_GRID (subsubgrid), GATE[i], 2, 1, 1, 1);
		g_signal_connect (GATE[i], "toggled", G_CALLBACK (control_xx_changed), (gpointer)(voice_index + i));

		RING[i] = gtk_toggle_button_new_with_label ("RingMod");
		gtk_grid_attach (GTK_GRID (subsubgrid), RING[i], 3, 1, 1, 1);
		g_signal_connect (RING[i], "toggled", G_CALLBACK (control_xx_changed), (gpointer)(voice_index + i));

		Triangle[i] = gtk_toggle_button_new_with_label ("Triangle/Ring");
		gtk_grid_attach (GTK_GRID (subsubgrid), Triangle[i], 0, 2, 1, 1);
		g_signal_connect (Triangle[i], "toggled", G_CALLBACK (control_xx_changed), (gpointer)(voice_index + i));

		Saw[i] = gtk_toggle_button_new_with_label ("Sawtooth");
		gtk_grid_attach (GTK_GRID (subsubgrid), Saw[i], 1, 2, 1, 1);
		g_signal_connect (Saw[i], "toggled", G_CALLBACK (control_xx_changed), (gpointer)(voice_index + i));

		Pulse[i] = gtk_toggle_button_new_with_label ("Pulse");
		gtk_grid_attach (GTK_GRID (subsubgrid), Pulse[i], 2, 2, 1, 1);
		g_signal_connect (Pulse[i], "toggled", G_CALLBACK (control_xx_changed), (gpointer)(voice_index + i));

		Noise[i] = gtk_toggle_button_new_with_label ("Noise");
		gtk_grid_attach (GTK_GRID (subsubgrid), Noise[i], 3, 2, 1, 1);
		g_signal_connect (Noise[i], "toggled", G_CALLBACK (control_xx_changed), (gpointer)(voice_index + i));

		temp = gtk_label_new ("Attack"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 3, 1, 1);
		Attack[i] = gtk_combo_box_new_with_model (GTK_TREE_MODEL (AttackModel));
		gtk_grid_attach (GTK_GRID (subgrid), Attack[i], 1, 3, 1, 1);
		{
			GtkCellRenderer *renderer;
			GtkTreePath *path;
			GtkTreeIter iter;
			renderer = gtk_cell_renderer_text_new ();
			gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (Attack[i]), renderer, TRUE);
			gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (Attack[i]), renderer, "text", 0, NULL);
			path = gtk_tree_path_new_from_indices (0, -1);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (AttackModel), &iter, path);
			gtk_tree_path_free (path);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (Attack[i]), &iter);
		}
		g_signal_connect(Attack[i], "changed", G_CALLBACK(AttackDecay_changed), (gpointer)(voice_index + i));

		temp = gtk_label_new ("Decay"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 4, 1, 1);
		Decay[i] = gtk_combo_box_new_with_model (GTK_TREE_MODEL (DecayReleaseModel));
		gtk_grid_attach (GTK_GRID (subgrid), Decay[i], 1, 4, 1, 1);
		{
			GtkCellRenderer *renderer;
			GtkTreePath *path;
			GtkTreeIter iter;
			renderer = gtk_cell_renderer_text_new ();
			gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (Decay[i]), renderer, TRUE);
			gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (Decay[i]), renderer, "text", 0, NULL);
			path = gtk_tree_path_new_from_indices (0, -1);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (DecayReleaseModel), &iter, path);
			gtk_tree_path_free (path);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (Decay[i]), &iter);
		}
		g_signal_connect(Decay[i], "changed", G_CALLBACK(AttackDecay_changed), (gpointer)(voice_index + i));

		temp = gtk_label_new ("Sustain"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 5, 1, 1);
		Sustain[i] = gtk_combo_box_new_with_model (GTK_TREE_MODEL (SustainModel));
		gtk_grid_attach (GTK_GRID (subgrid), Sustain[i], 1, 5, 1, 1);
		{
			GtkCellRenderer *renderer;
			GtkTreePath *path;
			GtkTreeIter iter;
			renderer = gtk_cell_renderer_text_new ();
			gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (Sustain[i]), renderer, TRUE);
			gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (Sustain[i]), renderer, "text", 0, NULL);
			path = gtk_tree_path_new_from_indices (0, -1);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (SustainModel), &iter, path);
			gtk_tree_path_free (path);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (Sustain[i]), &iter);
		}
		g_signal_connect(Sustain[i], "changed", G_CALLBACK(SustainRelease_changed), (gpointer)(voice_index + i));

		temp = gtk_label_new ("Release"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 6, 1, 1);
		Release[i] = gtk_combo_box_new_with_model (GTK_TREE_MODEL (DecayReleaseModel));
		gtk_grid_attach (GTK_GRID (subgrid), Release[i], 1, 6, 1, 1);
		{
			GtkCellRenderer *renderer;
			GtkTreePath *path;
			GtkTreeIter iter;
			renderer = gtk_cell_renderer_text_new ();
			gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (Release[i]), renderer, TRUE);
			gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (Release[i]), renderer, "text", 0, NULL);
			path = gtk_tree_path_new_from_indices (0, -1);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (DecayReleaseModel), &iter, path);
			gtk_tree_path_free (path);
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (Release[i]), &iter);
		}
		g_signal_connect(Release[i], "changed", G_CALLBACK(SustainRelease_changed), (gpointer)(voice_index + i));
	}

	{
		GtkWidget *temp, *frame, *subgrid, *subsubgrid;

		frame = gtk_frame_new ("Filters");
		gtk_widget_set_margin_top (frame, 10.0);
		gtk_widget_set_margin_bottom (frame, 10.0);
		gtk_widget_set_margin_start (frame, 10.0);
		gtk_widget_set_margin_end (frame, 10.0);

		gtk_grid_attach (GTK_GRID (thegrid), frame, 1, 0, 1, 3);

		subgrid = gtk_grid_new ();
		gtk_container_add (GTK_CONTAINER (frame), subgrid);
		
		temp = gtk_label_new ("Frequency"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 0, 1, 1);
		FC = gtk_spin_button_new_with_range (0x000, 0x7ff, 0x001);
		gtk_grid_attach (GTK_GRID (subgrid), FC, 1, 0, 1, 1);
		g_signal_connect (FC, "value-changed", G_CALLBACK (fc_value_changed), NULL);

		temp = gtk_label_new ("Resonance"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 1, 1, 1);
		RES = gtk_spin_button_new_with_range (0x0, 0xf, 0x1);
		gtk_grid_attach (GTK_GRID (subgrid), RES, 1, 1, 1, 1);
		g_signal_connect (RES, "value-changed", G_CALLBACK (rc_filt_value_changed), NULL);

		subsubgrid = gtk_grid_new ();
		gtk_grid_set_column_homogeneous (GTK_GRID (subsubgrid), TRUE);
		gtk_grid_attach (GTK_GRID (subgrid), subsubgrid, 0, 2, 2, 1);

		LP = gtk_toggle_button_new_with_label ("Low-Pass");
		gtk_grid_attach (GTK_GRID (subsubgrid), LP, 0, 1, 1, 1);
		g_signal_connect(LP, "toggled", G_CALLBACK(mode_vol_changed), NULL);

		BP = gtk_toggle_button_new_with_label ("Band-Pass");
		gtk_grid_attach (GTK_GRID (subsubgrid), BP, 1, 1, 1, 1);
		g_signal_connect(BP, "toggled", G_CALLBACK(mode_vol_changed), NULL);

		HP = gtk_toggle_button_new_with_label ("Hi-Pass");
		gtk_grid_attach (GTK_GRID (subsubgrid), HP, 2, 1, 1, 1);
		g_signal_connect(HP, "toggled", G_CALLBACK(mode_vol_changed), NULL);

		_3_OFF = gtk_toggle_button_new_with_label ("Voice 3 OFF");
		gtk_grid_attach (GTK_GRID (subgrid), _3_OFF, 0, 3, 2, 1);
		g_signal_connect(_3_OFF, "toggled", G_CALLBACK(mode_vol_changed), NULL);

		FILT[0] = gtk_toggle_button_new_with_label ("Filter Voice 1");
		gtk_grid_attach (GTK_GRID (subgrid), FILT[0], 0, 4, 2, 1);
		g_signal_connect (FILT[0], "toggled", G_CALLBACK (rc_filt_value_changed), NULL);

		FILT[1] = gtk_toggle_button_new_with_label ("Filter Voice 2");
		gtk_grid_attach (GTK_GRID (subgrid), FILT[1], 0, 5, 2, 1);
		g_signal_connect (FILT[1], "toggled", G_CALLBACK (rc_filt_value_changed), NULL);

		FILT[2] = gtk_toggle_button_new_with_label ("Filter Voice 3");
		gtk_grid_attach (GTK_GRID (subgrid), FILT[2], 0, 6, 2, 1);
		g_signal_connect (FILT[2], "toggled", G_CALLBACK (rc_filt_value_changed), NULL);

		temp = gtk_label_new ("Global Volume"); gtk_label_set_xalign (GTK_LABEL(temp), 0.0);
		gtk_grid_attach (GTK_GRID (subgrid), temp, 0, 7, 1, 1);
		VOL = gtk_spin_button_new_with_range (0x0, 0xf, 0x1);
		gtk_grid_attach (GTK_GRID (subgrid), VOL, 1, 7, 1, 1);
		g_signal_connect(VOL, "changed", G_CALLBACK(mode_vol_changed), NULL);
	}

	graph = gtk_drawing_area_new ();
	gtk_grid_attach (GTK_GRID (thegrid), graph, 2, 0, 1, 3);
	g_signal_connect (graph, "draw", G_CALLBACK (graph_draw), NULL);

	gtk_widget_set_size_request (graph, 256, 256);
	
	gtk_widget_show_all (window);


	for (i=0; i < 3; i++)
	{
		gtk_combo_box_set_active (GTK_COMBO_BOX(Attack[i]), 10);
		gtk_combo_box_set_active (GTK_COMBO_BOX(Decay[i]), 15);
		gtk_combo_box_set_active (GTK_COMBO_BOX(Sustain[i]), 10);
		gtk_combo_box_set_active (GTK_COMBO_BOX(Release[i]), 10);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (Saw[i]), TRUE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (FREQ[i]), 4000);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (PW[i]), 1023);
	}

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (VOL), 15.0);

}

int
main (int    argc,
      char **argv)
{
	GtkApplication *app;
	int status, i;

	app = gtk_application_new ("mywave.sidsynth", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	sid_instance_done();

	sidpulse_done ();

	return status;
}
