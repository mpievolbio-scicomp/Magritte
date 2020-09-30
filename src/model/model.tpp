#include <cmath>
#include <Eigen/Core>
#include <Eigen/Dense>
#include <cfloat>

//NOTE: i have mistakenly called tetrahedra triangles throughout this entire piece of code

// Calculates the relative difference of the density with respect to the neighbours
//    @Returns: abs((d-d_other)/(d+d_other)) such that the smallest values will be assigned
// to the points we want to delete
inline double Model :: calc_diff_abundance_with_neighbours(int point, int next_coars_lvl)
{
  double temp_max=0;//current implementation is probably slow
  for (auto& neigbor:neighbors_lists[next_coars_lvl].get_neighbors(point))
    {
      double abundance_self=chemistry.species.abundance[point][0];
      double abundance_other=chemistry.species.abundance[neigbor][0];
      double temp_diff=std::abs((abundance_self-abundance_other)/(abundance_self+abundance_other));
      temp_max=std::max(temp_max, temp_diff);
    }
    return temp_max;
}


///Helper function for deleting stuff from the density_diff_maps
///   @Param[in/out] std_map: the map with points as keys
///   @Param[in/out] reverse_map: the map with points as values
///   @Param[in] point_to_del: the point to delete from both maps
inline void delete_int_from_both_maps(std::multimap<int,double> &std_map, std::multimap<double,int> &reverse_map, int point_to_del)
{
  double curr_value=std_map.find(point_to_del).second;
  std_map.erase(point_to_del);//key is unique, so no problem
  //there are multiple points that currently have the same diff value, so we must iterate
  std::pair<iterator, iterator> iterpair = reverse_map.equal_range(curr_value);
  iterator it = iterpair.first;
  for (; it != iterpair.second; ++it) {
    if (it->second == point_to_del) {
      reverse_map.erase(it);
      break;
    }
  }
}

///Helper function for deleting stuff from the ears_maps
///   @Param[in/out] std_map: the map with points as keys
///   @Param[in/out] reverse_map: the map with points as values
///   @Param[in] point_to_del: the point to delete from both maps
///   @Returns: Returns an iterator for the standard map (needed for being able to delete while iterating)
inline iterator delete_vector_from_both_maps(std::multimap<vector<int>,double> &std_map, std::multimap<double,vector<int>> &reverse_map, vector<int> point_to_del)
{
  double curr_value=std_map.find(point_to_del).second;
  iterator it2=std_map.erase(point_to_del);//key is unique, so no problem
  //there are multiple points that currently have the same diff value, so we must iterate
  std::pair<iterator, iterator> iterpair = reverse_map.equal_range(curr_value);
  iterator it = iterpair.first;
  for (; it != iterpair.second; ++it) {
    if (it->second == point_to_del) {
      reverse_map.erase(it);
      break;
    }
  }
  return it2;
}

///Helper function: checks whether a vector contains an element
///   @Param[in] vect: the vector to check for
///   @Param[in] element: the element to check for
///   @Returns bool: true if element in vect, false otherwise
inline bool vector_contains_element(const vector<int> vect, int element)
{
  return (std::find(vect.begin(), vect.end(), element) != vect.end());
}



/// Given a tetrahedron, this calculates the power of the point with respect to the circumsphere
/// @Parameter [in] triangle: the tetrahedron from which we use the circumsphere
/// @Para
/// returns a positive value if the point is inside the circumsphere and a negative value if the point lies outside the circumsphere (zero if on the circumsphere)
inline double Model::calc_power(const vector<int> &triangle, int point){
  Vector3d pos1=geometry.points.position[triangle[0]];
  Vector3d pos2=geometry.points.position[triangle[1]];
  Vector3d pos3=geometry.points.position[triangle[2]];
  Vector3d pos4=geometry.points.position[triangle[3]];
  Vector3D posp=geometry.points.position[point];//position of point

  //dividing insphere test with orientation test
  Matrix<double,5,5> insphere;
  insphere << pos1[0],pos2[0],pos3[0],pos4[0],posp[0],
              pos1[1],pos2[1],pos3[1],pos4[1],posp[1],
              pos1[2],pos2[2],pos3[2],pos4[2],posp[2],
              pos1.squaredNorm(),pos2.squaredNorm(),pos3.squaredNorm(),pos4.squaredNorm(),posp.squaredNorm(),
              1,1,1,1,1;
  Matrix<double,4,4> orient;
  orient << pos1[0],pos2[0],pos3[0],pos4[0],
            pos1[1],pos2[1],pos3[1],pos4[1],
            pos1[2],pos2[2],pos3[2],pos4[2],
            1,1,1,1;

  return insphere.determinant()/orient.determinant();
}



/// Coarsens the grid
///   @Param[in] perc_points_deleted: if the grid has not yet been coarsened to the next level,
/// then it determines the percentage of points deleted, otherwise does nothing
inline void Model :: coarsen_grid(const float perc_points_deleted)
{
  if (curr_coarsening_lvl==max_reached_coarsening_lvl)//if we truly need to refine the grid
  {
    if (max_reached_coarsening_lvl==0)//first time refining grid
      {
      current_nb_points=parameters.npoints();
      neighbors_lists.resize(2);
      neighbors_lists[0]=Neighbors(geometry.points.curr_neighbors);//TODO properly implement deep copy
      neighbors_lists[1]=Neighbors(geometry.points.curr_neighbors);//TODO properly implement deep copy
      for (int i=0; i<parameters.npoints(); i++)//calc 1-abs(d-d_other)/(d+d_other) for all points
        {
          double curr_diff=calc_diff_abundance_with_neighbours(i,1);
          density_diff_map.insert(std::pair<int,double>(i,curr_diff));
          rev_density_diff_map.insert(std::pair<double,int>(curr_diff,i));
        }
      }
      else
      {
        neighbors_lists.resize(max_reached_coarsening_lvl+1);
        neighbors_lists[max_reached_coarsening_lvl+1]=Neighbors(neighbors_lists[max_reached_coarsening_lvl]);//should be deep copy
      }
      max_reached_coarsening_lvl++;
      curr_coarsening_lvl++;
      //repeat n times:
      Size nb_points_to_remove=Size(perc_points_deleted*current_nb_points);
      for (Size i=0; i<nb_points_to_remove; i++)
      {
        int curr_point=(*rev_density_diff_map.begin()).second;//the current point to remove
        vector<Size> neighbors_of_point=neighbors_lists[curr_coarsening_lvl].get_neighbors(curr_point);
        for (int neighbor :neighbors_of_point)
        {//deleting the point from its neighbors, the point itself still has its neighbors (for now)
          neighbors_lists[curr_coarsening_lvl].delete_single_neighbor(neighbor,curr_point);
        }

        //first calculate the relevant neighbors of the neighbors of the 'deleted' point
        std::map<int, std::set<int>> neighbor_map;//stores the relevant neighbors; WARNING does not get updated while recalculating mesh around the current point
        // use only for initial ear maps
        vector<int> cpy_of_neighbors=std::sort(vector<int>(neighbors_of_point));
        for (int neighbor: neighbors_of_point)
        {//intersecting the neighbors of the neighbor with the neighbors
          vector<int> cpy_of_curr_neighbors=std::sort(vector<int>(
            neighbors_lists[curr_coarsening_lvl].get_neighbors(neighbor)));
          std::set<int> rel_neigbors_of_neighbor;//relevant neighbors
          std::set_intersection(cpy_of_neighbors.begin(),cpy_of_neighbors.end(),
                      cpy_of_curr_neighbors.begin(),cpy_of_curr_neighbors.end(),
                      std::inserter(rel_neigbors_of_neighbor,rel_neigbors_of_neighbor.begin()));
          neighbor_map.insert(neighbor,rel_neigbors_of_neighbor);
        }
        //calculating all possible new 'ears' by adding a single line
        //iterating over all lines neighbors_of_point[i],neighbors_of_point[j]
        std::multimap<vector<int>,double> ears_map;
        std::multimap<double,vector<int>> rev_ears_map;

        for (int i=0; neighbors_of_point.size(); i++)
        {
          for (int j=0; j<i; j++)
          {
            if (neighbor_map[i].find(j)=neighbor_map[i].end())//if those two points are not yet neighbors
            {
              std::set<int> temp_intersection;
              temp_intersection=std::set_intersection(neighbor_map[i].begin(),neighbor_map[i].end()
                            neighbor_map[j].begin(),neighbor_map[j].end(),
                            std::inserter(temp_intersection,temp_intersection.begin()))
              //then for every pair in the intersection of the neighbors of both points, check if they are neighbors
              //a better implementation is probably possible
              for (int point1: &temp_intersection)
              {
                for (int point2: &temp_intersection)
                {
                  if (point1<point2 && //just such that we do not have duplicates
                  vector_contains_element(neighbor_map[point1], point2))///if point1 and 2 are neighbors
                  {
                    vector<int> new_triangle=vector<int>(i,j,point1,point2);
                    double power=calc_power(new_triangle,curr_point);
                    ears_map.insert(new_triangle,power);
                    rev_ears_map.insert(power,new_triangle);
                    //invariant: the first two element of the vector should correspond to the neighbors we want to add
                  }
                }
              }
            }
          }
        }

        //now that we have all initial 'triangles', we can finally start to add them
        while(!ears_map.empty())
        {
          vector<int> triangle=rev_ears_map.begin().second;//triangle which we are currently adding to the mesh
          if (!vector_contains_element(neighbors_lists[curr_coarsening_lvl].get_neighbors(triangle[0]),triangle[1]))
          {//necessary because of 'hack' with setting absolute priority to ear which also tries to add the same neighbors
          neighbors_lists[curr_coarsening_lvl].add_single_neighbor(triangle[0], triangle[1]);
          neighbors_lists[curr_coarsening_lvl].add_single_neighbor(triangle[1], triangle[0]);
          //invariant: the first two element of the vector should correspond to the neighbors we want to add
          }
          delete_vector_from_both_maps(ears_map,rev_ears_map,triangle);
          //check which possible ears need to be deleted
          for (auto it = ears_map.cbegin(); it != ears_map.cend();)
            {
              vector<int> curr_triangle=(*it).first;//candidate triangle to check whether it needs to be changed/deleted
              //if this candidate triangle would try to add the same neighbors, we just delete it (although i'm not sure whether it can happen before the mesh is almost complete...)
              if ((curr_triangle[0]==triangle[0]&&curr_triangle[1]==triangle[1])||
                  (curr_triangle[1]==triangle[0]&&curr_triangle[0]==triangle[1]))
              {
                double curr_power=(*it).second;
                if (curr_power!=-DBL_MAX)//infinite loop protection
                {
                it = delete_vector_from_both_maps(ears_map, rev_ears_map, curr_triangle);
                ears_map.insert(curr_triangle,-DBL_MAX);
                rev_ears_map.insert(-DBL_MAX,curr_triangle);//giving maximum priority to this triangle
                }//a better solution would be to also recalulte all other triangles which share a plane, but this requires some refactoring
              }
              else
              {
                std::set<int> union_of_points;
                //insert all points in a set; easier to work with
                for (int temp_point: &triangle)
                {
                  union_of_points.insert(temp_point);
                }
                for (int temp_point: &curr_triangle)
                {
                  union_of_points.insert(temp_point);
                }
                //if it shares a plane (and therefor size of union is (8-3)), delete them
                if (union_of_points.size()==5)
                  {
                  it = delete_vector_from_both_maps(ears_map, rev_ears_map, curr_triangle);
                  }
                else//we do nothing with the current entry
                {
                ++it;
                }
              }
              vector<int> plane1{triangle[0],triangle[1],triangle[2]};
              vector<int> plane2{triangle[0],triangle[1],triangle[3]};
              int count_neighbours_pl1;//counts the number of neighbors with plane 1
              int count_neighbours_pl2;//counts the number of neighbors with plane 2
              int point_not_neighbors_pl1;//just to not need to recalc which point of pl1 is not a neighbor
              int point_not_neighbors_pl2;

              //maybe put this into a seperate function
              //now that we have deleted all redundant ears, it is time to readd some new ears based on the new planes created
              for (int temp_point: neighbors_of_point)/
              { //we want to create new triangles, not just the current triangle again...
                if (!vector_contains_element(triangle,temp_point))
                {
                  count_neighbours_pl1=0;
                  count_neighbours_pl2=0;
                  vector<Size> neighbor_list_of_temp_point=neighbors_lists[curr_coarsening_lvl].get_neighbors(temp_point);
                  for (int point_of_pl1: plane1)
                  {
                    if (vector_contains_element(neighbor_list_of_temp_point,point_of_pl1))
                    {count_neighbours_pl1++;}
                  }
                  for (int point_of_pl2: plane2)
                  {
                    if (vector_contains_element(neighbor_list_of_temp_point,point_of_pl2))
                    {count_neighbours_pl2++;}
                  }
                  //if the candidate point is good for creating an ear with plane1
                  if (count_neighbours_pl1==2)
                  {
                    int point_not_neighbor_of_plane;//the point which is not a neighbor
                    vector<int> points_neighbor_of_plane;
                    points_neighbor_of_plane.reserve(2);
                    for (int point_of_pl1: &plane1)
                    {
                      if (!vector_contains_element(neighbor_list_of_temp_point,point_of_pl1)
                      {point_not_neighbor_of_plane=point_of_pl1;}
                      else
                      {points_neighbor_of_plane.push_back(point_of_pl1)}
                    }
                    //insert newly generated ear in maps
                    vector<int> new_possible_ear{temp_point,point_not_neighbor_of_plane,
                        points_neighbor_of_plane[0],points_neighbor_of_plane[1]};
                    double power=calc_power(new_possible_ear,curr_point);//TODO calculate this!!!!!
                    ears_map.insert(new_possible_ear,power);
                    rev_ears_map.insert(power,new_possible_ear);
                    }

                  //if the candidate point is good for creating an ear with plane1
                  if (count_neighbours_pl2==2)
                  {
                    int point_not_neighbor_of_plane;//the point which is not a neighbor
                    vector<int> points_neighbor_of_plane;
                    points_neighbor_of_plane.reserve(2);
                    for (int point_of_pl2: &plane2)
                    {
                      if (!vector_contains_element(neighbor_list_of_temp_point,point_of_pl2)
                      {point_not_neighbor_of_plane=point_of_pl2;}
                      else
                      {points_neighbor_of_plane.push_back(point_of_pl2)}
                    }
                    //insert newly generated ear in maps
                    vector<int> new_possible_ear{temp_point,point_not_neighbor_of_plane,
                        points_neighbor_of_plane[0],points_neighbor_of_plane[1]};
                    double power=calc_power(new_possible_ear,curr_point);
                    ears_map.insert(new_possible_ear,power);
                    rev_ears_map.insert(power,new_possible_ear);
                    }

                }
              }
            }
        }

        //recalc density diff of neighbors (due to new mesh (and ofc deletion of point))
        for (int neighbor :neighbors_of_point)
        {
          delete_int_from_both_maps(density_diff_map,rev_density_diff_map,neighbor);

          double calc_diff=calc_diff_abundance_with_neighbours(neighbor,curr_coarsening_lvl);
          density_diff_map.insert(std::pair<int,double>(neighbor,calc_diff));
          rev_density_diff_map.insert(std::pair<double,int>(calc_diff,neighbor));
        }

      //finally, delete the neighbors of this point
      neighbors_lists[curr_coarsening_lvl].delete_all_neighbors(curr_point);
      //and remove this entry from density maps
      delete_int_from_both_maps(density_diff_map, rev_density_diff_map, curr_point);
    }
    current_nb_points-=nb_points_to_remove;//decrement the number of points remaining
  }
  else
  {//TODO check whether deep copy!!!!!!
    curr_coarsening_lvl++;
    geometry.points.curr_neighbors=neighbors_lists[curr_coarsening_lvl];//should be shallow copy
  }
}





/// Resets the grid
inline void Model :: reset_grid()
{
  curr_coarsening_lvl=0;
  max_reached_coarsening_lvl=0;
  density_diff_map=std::multimap<int,double>();
  rev_density_diff_map=std::multimap<double,int>();
  geometry.points.curr_neighbors=Neighbors(neighbors_lists[0]);
  neighbors_lists=vector<Neighbors>();
  current_nb_points=0;
}
